//
// Distributed under the ITensor Library License, Version 1.2
//    (See accompanying LICENSE file.)
//
#include "itensor/itdata/itreal.h"
#include "itensor/itdata/itdata.h"
#include "itensor/itdata/itcplx.h"
#include "itensor/itdata/itlazy.h"
#include "itensor/indexset.h"
#include "itensor/util/count.h"
#include "itensor/tensor/sliceten.h"
#include "itensor/tensor/contract.h"
#include "itensor/tensor/lapack_wrap.h"

namespace itensor {

Cplx 
doTask(GetElt<Index> const& g, ITReal const& d)
    {
    return d[offset(g.is,g.inds)];
    }

void
doTask(SetElt<Real,Index> const& s, ITReal & d)
    {
    d[offset(s.is,s.inds)] = s.elt;
    }

void
doTask(SetElt<Cplx,Index> const& s, ITReal const& d, ManageStore & m)
    {
    auto nd = m.makeNewData<ITCplx>(d);
    nd->set(offset(s.is,s.inds),s.elt);
    }

void
doTask(FillReal const& f, ITReal & d)
    {
    std::fill(d.begin(),d.end(),f.r);
    }

void
doTask(FillCplx const& f, ITReal const& d, ManageStore & m)
    {
    m.makeNewData<ITCplx>(d.size(),f.z);
    }

void
doTask(MultCplx const& M, ITReal const& d, ManageStore & m)
    {
    auto nd = m.makeNewData<ITCplx>(d);
    (*nd) *= M.z;
    }

void
doTask(MultReal const& m, ITReal & d)
    {
    dscal_wrapper(d.size(),m.r,d.data());
    }

Real
doTask(NormNoScale, ITReal const& d) 
    { 
    return dnrm2_wrapper(d.size(),d.data());
    }

void
doTask(Conj,ITReal const& d) { /*Nothing to conj*/ }

void
doTask(TakeReal, ITReal const& d) { /*Already real*/ }

void
doTask(TakeImag, ITReal & d)
    { 
    //Set all elements to zero
    doTask(MultReal{0.},d);
    }

void
doTask(PrintIT<Index>& P, 
       ITReal const& d)
    {
    P.printInfo(d,"Dense Real",doTask(NormNoScale{},d));
     
    auto rank = P.is.r();
    if(rank == 0) 
        {
        P.s << "  ";
        P.printVal(P.scalefac*d.store.front());
        return;
        }

    if(!P.print_data) return;

    auto gc = detail::GCounter(rank);
    for(auto i : count(rank))
        gc.setRange(i,0,P.is.extent(i)-1);

    for(; gc.notDone(); ++gc)
        {
        auto val = P.scalefac*d[offset(P.is,gc.i)];
        if(std::norm(val) > Global::printScale())
            {
            P.s << "(";
            for(auto ii = gc.i.mini(); ii <= gc.i.maxi(); ++ii)
                {
                P.s << (1+gc[ii]);
                if(ii < gc.i.maxi()) P.s << ",";
                }
            P.s << ") ";

            P.printVal(val);
            }
        }
    }

Cplx
doTask(SumEls<Index>, ITReal const& d) 
    { 
    Real sum = 0;
    for(auto& elt : d) sum += elt;
    return sum;
    }

void
doTask(Write & W, ITReal const& d) 
    { 
    W.writeType(StorageType::ITReal,d); 
    }

void
doTask(Contract<Index> & C,
       ITReal const& a1,
       ITReal const& a2,
       ManageStore & m)
    {
    //if(not C.needresult)
    //    {
    //    m.makeNewData<ITLazy>(C.Lis,m.parg1(),C.Ris,m.parg2());
    //    return;
    //    }
    Label Lind,
          Rind,
          Nind;
    computeLabels(C.Lis,C.Lis.r(),C.Ris,C.Ris.r(),Lind,Rind);
    if(not C.Nis)
        {
        //Optimization TODO:
        //  Test different scenarios where having sortInds=true or false
        //  can improve performance. Having sorted inds can make adding
        //  quicker and let contractloop run in parallel more often in principle.
        bool sortInds = false; //whether to sort indices of result
        contractIS(C.Lis,Lind,C.Ris,Rind,C.Nis,Nind,sortInds);
        }
    else
        {
        Nind.resize(C.Nis.r());
        for(auto i : count(C.Nis.r()))
            {
            auto j = findindex(C.Lis,C.Nis[i]);
            if(j >= 0)
                {
                Nind[i] = Lind[j];
                }
            else
                {
                j = findindex(C.Ris,C.Nis[i]);
                Nind[i] = Rind[j];
                }
            }
        }
    auto t1 = makeTenRef(a1.data(),a1.size(),&C.Lis),
         t2 = makeTenRef(a2.data(),a2.size(),&C.Ris);
    auto rsize = area(C.Nis);
    START_TIMER(4)
    auto nd = m.makeNewData<ITReal>(rsize);
    STOP_TIMER(4)
    auto tr = makeTenRef(nd->data(),nd->size(),&(C.Nis));

    START_TIMER(2)
    //contractloop(t1,Lind,t2,Rind,tr,Nind);
    contract(t1,Lind,t2,Rind,tr,Nind);
    STOP_TIMER(2)

    START_TIMER(3)
    if(rsize > 1) C.scalefac = computeScalefac(*nd);
    STOP_TIMER(3)
    }

void
doTask(NCProd<Index>& P,
       ITReal const& d1,
       ITReal const& d2,
       ManageStore& m)
    {
    Label Lind,
          Rind,
          Nind;
    computeLabels(P.Lis,P.Lis.r(),P.Ris,P.Ris.r(),Lind,Rind);
    ncprod(P.Lis,Lind,P.Ris,Rind,P.Nis,Nind);

    auto t1 = makeTenRef(d1.data(),d1.size(),&P.Lis),
         t2 = makeTenRef(d2.data(),d2.size(),&P.Ris);
    auto rsize = area(P.Nis);
    auto nd = m.makeNewData<ITReal>(rsize);
    auto tr = makeTenRef(nd->data(),nd->size(),&(P.Nis));

    ncprod(t1,Lind,t2,Rind,tr,Nind);

    if(rsize > 1) P.scalefac = computeScalefac(*nd);
    }

void
doTask(PlusEQ<Index> const& P,
       ITReal & a1,
       ITReal const& a2)
    {
#ifdef DEBUG
    if(a1.size() != a2.size()) Error("Mismatched sizes in plusEq");
#endif
    if(isTrivial(P.perm()))
        {
        daxpy_wrapper(a1.size(),P.fac(),a2.data(),1,a1.data(),1);
        }
    else
        {
        auto ref1 = makeTenRef(a1.data(),a1.size(),&P.is1());
        auto ref2 = makeTenRef(a2.data(),a2.size(),&P.is2());
        auto f = P.fac();
        auto add = [f](Real r2, Real& r1) { r1 += f*r2; };
        transform(permute(ref2,P.perm()),ref1,add);
        }
    }

} // namespace itensor
