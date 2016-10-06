# Astrochem model example, written in Python

from astrochem.wrapper import *
import numpy as np

# Physical parameters
p = phys()
p.chi = 1.0
p.cosmic = 1.3-17

# Initial abundances
initial_abundances = {
    "H2": 0.5,
    "He": 0.14,
    "N": 2.14e-5,
    "O": 1.76e-4,
    "C(+)": 7.30e-5,
    "S(+)": 8.00e-8,
    "Si(+)": 8.00e-9,
    "Fe(+)": 3.00e-9,
    "Na(+)": 2.00e-9,
    "Mg(+)": 7.00e-9,
    "P(+)": 2.00e-10,
    "Cl(+)": 1.00e-9,
    "F": 6.68e-9,
    "e(-)": 7.31012e-5
}

# Source parameters
c = cell(av=20, nh=1e3, tgas=10, tdust=10)

# Solver parameters
verbose = 0
abs_err = 1e-15
rel_err = 1e-6
s = solver(c,  "../../../networks/osu2009.chm", p , abs_err, rel_err, initial_abundances, 1e3, verbose)

# Computation loop
time = np.logspace(-6, 7, 128) 
for ti in time:
    abundances = s.solve(ti, c)
    print abundances['CO']

