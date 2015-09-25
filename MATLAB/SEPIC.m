% SEPIC converter design
% Input source voltage (Vs), output voltage (Vo), output power (Po)
% switching frequency (f), capacitor 1 ripple voltage (C1_ripple),
% and output voltage ripple (Vo_ripple)
% Input ripple voltages as a decimal value between 0 and 1

function [L1_min L2_min C1_min C2_min] = SEPIC(Vs,Vo,Po,f,C1_ripple,Vo_ripple)

% Calculate duty ratio
D = Vo/(Vo + Vs);

% Calculate resistor value
R = Vo^2/Po;

% Solve for inductor values
L1_min = (D*Vs^2)/(2*Po*f);
L2_min = (D*Vo*Vs)/(Po*f);

% Solve for capacitor values
C1_min = D/(R*C1_ripple*f);
C2_min = D/(R*Vo_ripple*f);
end
