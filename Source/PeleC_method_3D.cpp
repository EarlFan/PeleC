#include "PeleC_method_3D.H" 

//Host function to call gpu hydro functions
void PeleC_umeth_3D(amrex::Box const& bx, const int* bclo, const int* bchi, 
           const int* domlo, const int* domhi, 
           amrex::Array4<const amrex::Real> const &q, 
           amrex::Array4<const amrex::Real> const& qaux,
           amrex::Array4<const amrex::Real> const& srcQ,// amrex::IArrayBox const& bcMask,
           amrex::Array4<amrex::Real> const& flx1, amrex::Array4<amrex::Real> const& flx2,
           amrex::Array4<amrex::Real> const& flx3, // amrex::Array4<const amrex::Real> const& dloga,
           amrex::Array4<amrex::Real> const& q1, amrex::Array4< amrex::Real> const& q2, 
           amrex::Array4<amrex::Real> const& q3, amrex::Array4<const amrex::Real> const& a1, 
           amrex::Array4<const amrex::Real> const& a2, amrex::Array4<const amrex::Real> const& a3, 
           amrex::Array4<amrex::Real> const& pdivu, amrex::Array4<const amrex::Real> const& vol,
           const amrex::Real *del, const amrex::Real dt)
{
    amrex::Real const dx    = del[0]; 
    amrex::Real const dy    = del[1]; 
    amrex::Real const dz    = del[2]; 
    amrex::Real const hdtdx = 0.5*dt/dx; 
    amrex::Real const hdtdy = 0.5*dt/dy; 
    amrex::Real const hdtdz = 0.5*dt/dz;
    amrex::Real const cdtdx = 1.0/3.0*dt/dx; 
    amrex::Real const cdtdy = 1.0/3.0*dt/dy; 
    amrex::Real const cdtdz = 1.0/3.0*dt/dz;  
    amrex::Real const hdt   = 0.5*dt; 


    const int bclx = bclo[0]; 
    const int bcly = bclo[1];
    const int bclz = bclo[2];  
    const int bchx = bchi[0]; 
    const int bchy = bchi[1]; 
    const int bchz = bchi[2]; 
    const int dlx  = domlo[0]; 
    const int dly  = domlo[1];
    const int dlz  = domlo[2];      
    const int dhx  = domhi[0]; 
    const int dhy  = domhi[1]; 
    const int dhz  = domhi[2]; 
    
//    auto const& bcMaskarr = bcMask.array();
    const Box& bxg1 = grow(bx, 1); 
    const Box& bxg2 = grow(bx, 2);
    AsyncFab qmfab(bxg2, QVAR); 
    AsyncFab qpfab(bxg1, QVAR);
    auto const& qm = qmfab.array(); 
    auto const& qp = qpfab.array();  
    AsyncFab slope(bxg2, QVAR);
    auto const& slarr = slope.array();

//===================== X data  ===================================

    int cdir = 0; 
    const Box& xmbx = growHi(bxg2, cdir, 1); 
    AsyncFab qxm(xmbx, QVAR); 
    AsyncFab qxp(bxg2, QVAR);
    auto const& qxmarr = qxm.array(); 
    auto const& qxparr = qxp.array();

//===================== Y data  ===================================

    cdir = 1; 
    const Box& yflxbx = surroundingNodes(grow(bxg2, cdir, -1) ,cdir); 
    const Box& ymbx = growHi(bxg2, cdir, 1); 
    AsyncFab qym(ymbx, QVAR);
    AsyncFab qyp(bxg2, QVAR);
    auto const& qymarr = qym.array(); 
    auto const& qyparr = qyp.array();  

//===================== Z data  ===================================

    cdir = 2; 
    const Box& zmbx = growHi(bxg2, cdir, 1); 
    const Box& zflxbx = surroundingNodes(grow(bxg2,cdir, -1),cdir);
    AsyncFab qzm(zmbx, QVAR); 
    AsyncFab qzp(bxg2, QVAR);
    auto const& qzmarr = qzm.array(); 
    auto const& qzparr = qzp.array();


    AMREX_PARALLEL_FOR_3D (bxg2,i,j,k, {
//===================== X slopes ===================================
        for(int n = 0; n < QVAR; ++n) 
            PeleC_slope_x(i,j,k,n, slarr, q);
//==================== X interp ====================================
        PeleC_plm_x(i, j, k, qxmarr, qxparr, slarr, q, qaux(i,j,k,QC), 
                  dx, dt);
//==================== Y slopes ====================================
        for(int n = 0; n < QVAR; n++) 
            PeleC_slope_y(i, j, k, n, slarr, q);
//==================== Y interp ====================================
        PeleC_plm_y(i,j,k, qymarr, qyparr, slarr, q, qaux(i,j,k,QC), 
                    dy, dt);
//===================== Z slopes ===================================
      for(int n = 0; n < QVAR; ++n) 
        PeleC_slope_z(i,j,k,n, slarr, q);
//==================== Z interp ====================================
      PeleC_plm_z(i, j, k, qzmarr, qzparr, slarr, q, qaux(i,j,k,QC), 
                   dz, dt);
    }); 

//===================== X initial fluxes ===========================
    cdir = 0; 
    const Box& xflxbx = surroundingNodes(grow(bxg2, cdir, -1), cdir); 
    AsyncFab fx(xflxbx, NVAR);
    auto const& fxarr = fx.array(); 
    AsyncFab qgdx(xflxbx, NGDNV); 
    auto const& gdtempx = qgdx.array(); 

    AMREX_PARALLEL_FOR_3D (xflxbx, i,j,k, {
        PeleC_cmpflx(i,j,k,bclx, bchx, dlx, dhx, qxmarr, qxparr, fxarr, gdtempx, qaux, cdir);
    });

//===================== Y initial fluxes ===========================
    cdir = 1; 

    AsyncFab fy(yflxbx, NVAR); 
    auto const& fyarr = fy.array();
    AsyncFab qgdy(yflxbx, NGDNV); 
    auto const& gdtempy = qgdy.array(); 
    AMREX_PARALLEL_FOR_3D (yflxbx, i,j,k, {
        PeleC_cmpflx(i,j,k, bcly, bchy, dly, dhy, qymarr, qyparr, fyarr, gdtempy, qaux, cdir); 
    }); 

//===================== Z initial fluxes ===========================
    cdir = 2; 
    AsyncFab fz(zflxbx, NVAR);
    auto const& fzarr = fz.array(); 
    AsyncFab qgdz(zflxbx, NGDNV); 
    auto const& gdtempz = qgdz.array(); 
    AMREX_PARALLEL_FOR_3D (zflxbx, i,j,k, {
        PeleC_cmpflx(i,j,k,bclz, bchz, dlz, dhz, qzmarr, qzparr, fzarr, gdtempz, qaux, cdir);
    });


//===================== X interface corrections ====================

    cdir = 0;
    const Box& txbx = grow(bxg1, cdir, 1);
    const Box& txbxm = growHi(txbx, cdir, 1); 
    AsyncFab qxym(txbxm, QVAR); 
    AsyncFab qxyp(txbx , QVAR);
    auto const& qmxy = qxym.array();
    auto const& qpxy = qxyp.array();  

    AsyncFab qxzm(txbxm, QVAR); 
    AsyncFab qxzp(txbx , QVAR);
    auto const& qmxz = qxzm.array();
    auto const& qpxz = qxzp.array();  
    AMREX_PARALLEL_FOR_3D (txbx, i,j,k, {
// ----------------- X|Y ------------------------------------------
        PeleC_transy1(i,j,k, qmxy, qpxy, qxmarr, qxparr, fyarr,
                     srcQ, qaux, gdtempy, a2, vol, hdt, cdtdy);


// ---------------- X|Z ------------------------------------------
        PeleC_transz1(i,j,k, qmxz, qpxz, qxmarr, qxparr, fzarr,
                     srcQ, qaux, gdtempz, a3, vol, hdt, cdtdz);

   });

   const Box& txfxbx = surroundingNodes(bxg1, cdir); 
   AsyncFab fluxxy(txfxbx, NVAR); 
   AsyncFab fluxxz(txfxbx, NVAR); 
   AsyncFab gdvxyfab(txfxbx, NGDNV); 
   AsyncFab gdvxzfab(txfxbx, NGDNV); 
    
   auto const& flxy = fluxxy.array(); 
   auto const& flxz = fluxxz.array(); 
   auto const& qxy = gdvxyfab.array(); 
   auto const& qxz = gdvxzfab.array();  

//===================== Riemann problem X|Y X|Z ====================

    AMREX_PARALLEL_FOR_3D (txfxbx, i,j,k, {     
// -----------------  X|Y ---------------------------------------------------       
      PeleC_cmpflx(i,j,k, bclx, bchx, dlx, dhx, qmxy, qpxy, flxy, qxy, qaux, cdir); 
// -----------------  X|Z --------------------------------------------------
      PeleC_cmpflx(i,j,k, bclx, bchx, dlx, dhx, qmxz, qpxz, flxz, qxz, qaux, cdir); 
    }); 

//===================== Y interface corrections ====================

    cdir = 1; 
    const Box& tybx  = grow(bxg1, cdir, 1);
    const Box& tybxm = growHi(tybx, cdir, 1); 
    AsyncFab qyxm(tybxm, QVAR); 
    AsyncFab qyxp(tybx, QVAR); 
    AsyncFab qyzm(tybxm, QVAR); 
    AsyncFab qyzp(tybx, QVAR); 
    auto const& qmyx = qyxm.array(); 
    auto const& qpyx = qyxp.array(); 
    auto const& qmyz = qyzm.array(); 
    auto const& qpyz = qyzp.array(); 

    AMREX_PARALLEL_FOR_3D (tybx, i, j , k, {
//--------------------- Y|X -------------------------------------
        PeleC_transx1(i,j,k, qmyx, qpyx, qymarr, qyparr, fxarr,
                     srcQ, qaux, gdtempx, a1, vol, hdt, cdtdx); 

//--------------------- Y|Z ------------------------------------              
        PeleC_transz2(i,j,k, qmyz, qpyz, qymarr, qyparr, fzarr, 
                     srcQ, qaux, gdtempz, a3, vol, hdt, cdtdz); 
    }); 

//===================== Riemann problem Y|X Y|Z  ====================

   const Box& tyfxbx = surroundingNodes(bxg1, cdir);
   AsyncFab fluxyx(tyfxbx, NVAR); 
   AsyncFab fluxyz(tyfxbx, NVAR); 
   AsyncFab gdvyxfab(tyfxbx, NGDNV); 
   AsyncFab gdvyzfab(tyfxbx, NGDNV); 
    
   auto const& flyx = fluxyx.array(); 
   auto const& flyz = fluxyz.array(); 
   auto const& qyx = gdvyxfab.array(); 
   auto const& qyz = gdvyzfab.array();  
   
    AMREX_PARALLEL_FOR_3D (tyfxbx, i, j, k, {
//--------------------- Y|X ----------------------------------------------------
      PeleC_cmpflx(i,j,k, bcly, bchy, dly, dhy, qmyx , qpyx , flyx, qyx, qaux, cdir);

//--------------------- Y|Z ----------------------------------------------------
      PeleC_cmpflx(i,j,k, bcly, bchy, dly, dhy, qmyz , qpyz , flyz, qyz, qaux, cdir); 
    });

//===================== Z interface corrections ====================

    cdir = 2; 
    const Box& tzbx = grow(bxg1, cdir, 1);
    const Box& tzbxm = growHi(tzbx, cdir, 1);
    AsyncFab qzxm(tzbxm, QVAR); 
    AsyncFab qzxp(tzbx, QVAR); 
    AsyncFab qzym(tzbxm, QVAR); 
    AsyncFab qzyp(tzbx, QVAR); 
    auto const& qmzx = qzxm.array(); 
    auto const& qpzx = qzxp.array(); 
    auto const& qmzy = qzym.array(); 
    auto const& qpzy = qzyp.array(); 

    AMREX_PARALLEL_FOR_3D (tzbx, i, j , k, {
//------------------- Z|X -------------------------------------
        PeleC_transx2(i,j,k, qmzx, qpzx, qzmarr, qzparr, fxarr,
                     srcQ, qaux, gdtempx, a1, vol, hdt, cdtdx);

//------------------- Z|Y -------------------------------------                
        PeleC_transy2(i,j,k, qmzy, qpzy, qzmarr, qzparr, fyarr, 
                     srcQ, qaux, gdtempy, a2, vol, hdt, cdtdy); 
    }); 
//===================== Riemann problem Z|X Z|Y  ====================
    
   const Box& tzfxbx = surroundingNodes(bxg1, cdir);
   AsyncFab fluxzx(tzfxbx, NVAR); 
   AsyncFab fluxzy(tzfxbx, NVAR); 
   AsyncFab gdvzxfab(tzfxbx, NGDNV); 
   AsyncFab gdvzyfab(tzfxbx, NGDNV); 
    
   auto const& flzx = fluxzx.array(); 
   auto const& flzy = fluxzy.array(); 
   auto const& qzx = gdvzxfab.array(); 
   auto const& qzy = gdvzyfab.array(); 

   AMREX_PARALLEL_FOR_3D (tzfxbx, i, j, k, {
//-------------------- Z|X -----------------------------------------------------
      PeleC_cmpflx(i,j,k, bclz, bchz, dlz, dhz, qmzx, qpzx, flzx, qzx, qaux, cdir);
//-------------------- Z|Y -----------------------------------------------------
      PeleC_cmpflx(i,j,k, bclz, bchz, dlz, dhz, qmzy, qpzy, flzy, qzy, qaux, cdir); 
    });

//==================== X| Y&Z ======================================
    cdir = 0;   
    const Box& xfxbx = surroundingNodes(bx, cdir);
    const Box& tyzbx = grow(bx, cdir, 1); 
    AMREX_PARALLEL_FOR_3D ( tyzbx, i, j, k, { 
        PeleC_transyz(i,j,k, qm, qp, qxmarr, qxparr, flyz, flzy, qyz, qzy, qaux, srcQ, hdt, hdtdy, hdtdz); 
    });

//============== Final X flux ======================================
    AMREX_PARALLEL_FOR_3D(xfxbx, i, j, k, {
        PeleC_cmpflx(i,j,k,bclx, bchx, dlx, dhx, qm, qp, flx1, q1, qaux, cdir); 
    }); 

//==================== Y| X&Z ======================================
    cdir = 1;   
    const Box& yfxbx = surroundingNodes(bx, cdir);
    const Box& txzbx = grow(bx, cdir, 1); 
    AMREX_PARALLEL_FOR_3D (txzbx, i, j, k, { 
        PeleC_transxz(i,j,k, qm, qp, qymarr, qyparr, flxz, flzx, qxz, qzx, qaux, srcQ, hdt, hdtdx, hdtdz); 
    }); 

//============== Final Y flux ======================================
    AMREX_PARALLEL_FOR_3D(yfxbx, i, j, k, {
        PeleC_cmpflx(i,j,k,bcly, bchy, dly, dhy, qm, qp, flx2, q2, qaux, cdir); 
    }); 

//==================== Z| X&Y ======================================
    cdir = 2;   
    const Box& zfxbx = surroundingNodes(bx, cdir);
    const Box& txybx = grow(bx, cdir, 1); 
    AMREX_PARALLEL_FOR_3D ( txybx, i, j, k, { 
        PeleC_transxy(i,j,k, qm, qp, qzmarr, qzparr, flxy, flyx, qxy, qyx, qaux, srcQ, hdt, hdtdx, hdtdy);
/*        for(int n = 0; n < QVAR; n++){
            qm(i,j,k+1,n) = qzmarr(i,j,k+1,n); 
            qp(i,j,k,n) = qzparr(i,j,k,n);
        }  */
    }); 

//============== Final Z flux ======================================
    AMREX_PARALLEL_FOR_3D(zfxbx, i, j, k, {
        PeleC_cmpflx(i,j,k,bclz, bchz, dlz, dhz, qm, qp, flx3, q3, qaux, cdir); 
    }); 

//===================== Construct p div{U} =========================
    AMREX_PARALLEL_FOR_3D (bx, i, j, k, {
        PeleC_pdivu(i,j,k, pdivu, q1, q2, q3, a1, a2, a3, vol); 
    });
}
