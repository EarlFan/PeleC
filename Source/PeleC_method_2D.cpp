#include "PeleC_method_2D.H" 

//Host function to call gpu hydro functions
void PeleC_umeth_2D(amrex::Box const& bx, amrex::Array4<const amrex::Real> const &q, 
           amrex::Array4<const amrex::Real> const& qaux,
           amrex::Array4<const amrex::Real> const& srcQ, amrex::IArrayBox const& bcMask,
           amrex::Array4<amrex::Real> const& flx1, amrex::Array4<amrex::Real> const& flx2, 
           amrex::Array4<const amrex::Real> const& dloga, amrex::Array4<amrex::Real> const& q1,
           amrex::Array4<amrex::Real> const& q2, amrex::Array4<const amrex::Real> const& a1, 
           amrex::Array4<const amrex::Real> const& a2, amrex::Array4<amrex::Real> const& pdivu, 
           amrex::Array4<const amrex::Real> const& vol, const amrex::Real *dx, const amrex::Real dt)
{
    amrex::Real const dtdx  = dt/dx[0]; 
    amrex::Real const hdtdx = 0.5*dtdx; 
    amrex::Real const hdtdy = 0.5*dt/dx[1]; 
    amrex::Real const hdt   = 0.5*dt; 

    auto const& bcMaskarr = bcMask.array();
    const Box& bxg1 = grow(bx, 1); 
    const Box& bxg2 = grow(bx, 2);
    AsyncFab slope(bxg2, QVAR);
    auto const& slarr = slope.array();

//===================== X slopes ===================================
    int cdir = 0; 
    const Box& xslpbx = grow(bxg1, cdir, 1);

    IntVect lox(AMREX_D_DECL(xslpbx.loVect()[0], xslpbx.loVect()[1], xslpbx.loVect()[2])); 
    IntVect hix(AMREX_D_DECL(xslpbx.hiVect()[0]+1, xslpbx.hiVect()[1], xslpbx.hiVect()[2])); 
    const Box xmbx(lox, hix);
    const Box& xflxbx = surroundingNodes(bxg1,cdir);
    amrex::Print()<< "Slope x! " << std::endl; 
    AMREX_PARALLEL_FOR_3D (xslpbx,i,j,k, { 
        PeleC_slope_x(i,j,k, slarr, q);
    }); // */
    Gpu::Device::synchronize(); 
//==================== X interp ====================================
    AsyncFab qxm(xmbx, QVAR); 
    AsyncFab qxp(xslpbx, QVAR);
    auto const& qxmarr = qxm.array(); 
    auto const& qxparr = qxp.array(); 

    amrex::Print() << "PLM X" << std::endl;
    AMREX_PARALLEL_FOR_3D (xslpbx, i,j,k, {
       PeleC_plm_x(i, j, k, qxmarr, qxparr, slarr, q, qaux(i,j,k,QC) , 
                    srcQ, dx[0], dt); // dloga, dx[0], dt);
   });
   Gpu::Device::synchronize(); 

//===================== X initial fluxes ===========================
    AsyncFab fx(xflxbx, NVAR);
    auto const& fxarr = fx.array(); 

    //bcMaskarr at this point does nothing.  
    amrex::Print() << "FLUX X " << std::endl; 
    AMREX_PARALLEL_FOR_3D (xflxbx, i,j,k, {
        PeleC_cmpflx(i,j,k, qxmarr, qxparr, fxarr, q1, qaux,0);
                // bcMaskarr, 0);
    });
    Gpu::Device::synchronize(); 

//==================== Y slopes ====================================
    cdir = 1; 
    const Box& yflxbx = surroundingNodes(bxg1,cdir); 
    const Box& yslpbx = grow(bxg1, cdir, 1);
    IntVect loy(AMREX_D_DECL(yslpbx.loVect()[0], yslpbx.loVect()[1], yslpbx.loVect()[2])); 
    IntVect hiy(AMREX_D_DECL(yslpbx.hiVect()[0], yslpbx.hiVect()[1]+1, yslpbx.hiVect()[2])); 

    const Box ymbx(loy,hiy); 
    AsyncFab qym(ymbx, QVAR);
    AsyncFab qyp(yslpbx, QVAR);
    auto const& qymarr = qym.array(); 
    auto const& qyparr = qyp.array();  
    
    amrex::Print() << "Slope y !" << std::endl;  
    AMREX_PARALLEL_FOR_3D (yslpbx, i,j,k,{
        PeleC_slope_y(i,j,k, slarr, q); 
    }); // */
   Gpu::Device::synchronize(); 

//==================== Y interp ====================================
    amrex::Print() << "PLM Y " << std::endl; 
    AMREX_PARALLEL_FOR_3D (yslpbx, i,j,k, {
        PeleC_plm_y(i,j,k, qymarr, qyparr, slarr, q, qaux(i,j,k,QC), 
                srcQ, dx[1], dt); // dloga, dx[1], dt);
  });
   Gpu::Device::synchronize(); 

//===================== Y initial fluxes ===========================
    amrex::Print()<< " FLX Y " << std::endl; 
    AsyncFab fy(yflxbx, NVAR); 
    auto const& fyarr = fy.array();
    AMREX_PARALLEL_FOR_3D (yflxbx, i,j,k, {
        PeleC_cmpflx(i,j,k, qymarr, qyparr, fyarr, q2, qaux, 1); 
        // bcMaskarr, 1);
    }); 
   Gpu::Device::synchronize(); 

//===================== X interface corrections ====================
    cdir = 0; 
    AsyncFab qm(bxg2, QVAR); 
    AsyncFab qp(bxg2, QVAR);
    const Box& tybx = grow(bx, cdir, 1); 
    auto const& qmarr = qm.array();
    auto const& qparr = qp.array();  
    amrex::Print() << " Transy " << std::endl; 
    AMREX_PARALLEL_FOR_3D (tybx, i,j,k, {
        PeleC_transy(i,j,k, qmarr, qparr, qxmarr, qxparr, fyarr,
                     srcQ, qaux, q2, a2, vol, hdt, hdtdy);
   });
   Gpu::Device::synchronize(); 

//===================== Final Riemann problem X ====================
    const Box& xfxbx = surroundingNodes(bx, cdir); 
    amrex::Print() << "FLX x 2" << std::endl;
    AMREX_PARALLEL_FOR_3D (xfxbx, i,j,k, {      
      PeleC_cmpflx(i,j,k, qmarr, qparr, flx1, q1, qaux,0); // bcMaskarr, 0);
    }); 
    Gpu::Device::synchronize(); 

//===================== Y interface corrections ====================
    cdir = 1; 
    const Box& txbx = grow(bx, cdir, 1);
    amrex::Print() << "Transx" << std::endl; 
    AMREX_PARALLEL_FOR_3D (txbx, i, j , k, {
        PeleC_transx(i,j,k, qmarr, qparr, qymarr, qyparr, fxarr,
                     srcQ, qaux, q1, a1, vol, hdt, hdtdx);                
  });
  Gpu::Device::synchronize(); 

//===================== Final Riemann problem Y ====================
    
    const Box& yfxbx = surroundingNodes(bx, cdir);
    AMREX_PARALLEL_FOR_3D (yfxbx, i, j, k, {
      PeleC_cmpflx(i,j,k, qmarr, qparr, flx2, q2, qaux, 1); // bcMaskarr, 1); 
    });
    Gpu::Device::synchronize(); 

//===================== Construct p div{U} =========================
    amrex::Print() << "P divu " << std::endl; 
    AMREX_PARALLEL_FOR_3D (bx, i, j, k, {
        PeleC_pdivu(i,j,k, pdivu, q1, q2, a1, a2, vol); 
    });
    Gpu::Device::synchronize(); 

}

