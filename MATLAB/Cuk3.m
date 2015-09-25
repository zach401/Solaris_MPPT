Vsmax = 38;
Rmax = 20;
Rmin = 1.44;
Dmin = .24;
Dmax = .8;
f = 200e3;
dIL1 = 1;
dIL2 = 1;
Vc1_ripple = .1;
Vc2_ripple = .025;
L1 = ((Rmax*(1-Dmin)^2)/(2*Dmin*f)) %(Vsmax*Dmax^2)/(f*dIL1)   
L2 = ((Rmax*(1-Dmin))/(2*f))        %(Vsmax*Dmax)/(f*dIL2)     
C1 = (Dmax)/(Rmin*f*(Vc1_ripple))
C2 = (1-Dmin)/(Vc2_ripple*8*L2*f^2)


