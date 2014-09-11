/*
   solve.c - Build the ODE system and the jacobian matrix, and solve
   the system with CVODE.

   Copyright (c) 2006-2014 Sebastien Maret

   This file is part of Astrochem.

   Astrochem is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   Astrochem is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Astrochem.  If not, see <http://www.gnu.org/licenses/>.
   */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cvode/cvode.h>
#ifdef USE_LAPACK
#include <cvode/cvode_lapack.h>
#else
#include <cvode/cvode_dense.h>
#endif

#include "libastrochem.h"
#include "solve.h"

#include "rates.h"

static int f (realtype t, N_Vector y, N_Vector ydot, void *params);

static int jacobian (int N, realtype t, N_Vector y, N_Vector fy,
                     DlsMat J, void *params, N_Vector tmp1,
                     N_Vector tmp2, N_Vector tmp3);

/**
 * @brief fonction used in the ODE system.
 * @param t time
 * @param y output
 * @param ydot ydot
 * @param params user params
 * @return computed value
 */
static int
f (realtype t __attribute__ ((unused)), N_Vector y, N_Vector ydot,
   void *params)
{
  int i;

  /* Get the parameters passed with *params. */

  double *reac_rates = ((params_t *) params)->reac_rates;
  const react_t *reactions = ((params_t *) params)->reactions;
  int n_reactions = ((params_t *) params)->n_reactions;
  int n_species = ((params_t *) params)->n_species;
  double nh = ((params_t *) params)->nh;
  double av = ((params_t *) params)->av;
  double tgas = ((params_t *) params)->tgas;
  double tdust = ((params_t *) params)->tdust;
  double chi = ((params_t *) params)->chi;
  double cosmic = ((params_t *) params)->cosmic;
  double grain_size = ((params_t *) params)->grain_size;
  double grain_abundance = ((params_t *) params)->grain_abundance;

  /* Loop over the reactions and build the right hand ODE system
     equations. */

  for (i = 0; i < n_species; i++)
    {
      NV_Ith_S (ydot, i) = 0;
    }

  for (i = 0; i < n_reactions; i++)
    {
      double y_product;

      /* Compute the production/destruction rate of the reaction. */

      y_product = 1;

      if (reactions[i].reaction_type == 0)
        {
          /* The formation of H2 is a first order reaction, so the
             product of the reactants needs to be divided by the H
             abundance. */

          y_product *= NV_Ith_S (y, reactions[i].reactant1);
          y_product *= reac_rates[i];
        }
      else if (reactions[i].reaction_type == 23)
        {
          /* Photo-desorption is a zeroth order reaction. However the
             reaction rate depends on the ice thickness, so it must be
             recomputed at each time step. */

          reac_rates[i] = rate (reactions[i].alpha,
                                reactions[i].beta,
                                reactions[i].gamma,
                                reactions[i].reaction_type,
                                reactions[i].reaction_no,
                                nh, av, tgas, tdust,
                                chi, cosmic,
                                grain_size,
                                grain_abundance,
                                NV_Ith_S (y, reactions[i].reactant1) / nh);
          y_product = reac_rates[i];
        }
      else
        {
          /* For other reactions, the production/destruction rate is
             the product of the reactants multiplied by the reaction rate. */

          if (reactions[i].reactant1 != -1)
            y_product *= NV_Ith_S (y, reactions[i].reactant1);
          if (reactions[i].reactant2 != -1)
            y_product *= NV_Ith_S (y, reactions[i].reactant2);
          if (reactions[i].reactant3 != -1)
            y_product *= NV_Ith_S (y, reactions[i].reactant3);
          y_product *= reac_rates[i];
        }

      /* Add a production term for each product of the reaction. */

      if (reactions[i].product1 != -1)
        NV_Ith_S (ydot, reactions[i].product1) += y_product;
      if (reactions[i].product2 != -1)
        NV_Ith_S (ydot, reactions[i].product2) += y_product;
      if (reactions[i].product3 != -1)
        NV_Ith_S (ydot, reactions[i].product3) += y_product;
      if (reactions[i].product4 != -1)
        NV_Ith_S (ydot, reactions[i].product4) += y_product;

      /* Add a destruction term for each reactant of the reaction. */

      if (reactions[i].reactant1 != -1)
        NV_Ith_S (ydot, reactions[i].reactant1) -= y_product;
      if (reactions[i].reactant2 != -1)
        NV_Ith_S (ydot, reactions[i].reactant2) -= y_product;
      if (reactions[i].reactant3 != -1)
        NV_Ith_S (ydot, reactions[i].reactant3) -= y_product;
    }

  return (0);
}

/**
 * @brief compute jacobian matrix
 * @param N unused attribute
 * @param y output
 * @param fy unsed attribute
 * @param[out] jacobian output matrix
 * @param params user params
 * @param tmp1 unsed attribute
 * @param tmp2 unsed attribute
 * @param tmp3 unsed attribute
 * @return 0
 */
static int
jacobian (int N __attribute__ ((unused)),
          realtype t __attribute__ ((unused)), N_Vector y,
          N_Vector fy __attribute__ ((unused)),
          DlsMat J, void *params,
          N_Vector tmp1 __attribute__ ((unused)),
          N_Vector tmp2 __attribute__ ((unused)),
          N_Vector tmp3 __attribute__ ((unused)))
{
  int i, j;

  /* Get the parameters passed with *params. */

  double *reac_rates = ((params_t *) params)->reac_rates;
  const react_t *reactions = ((params_t *) params)->reactions;
  int n_reactions = ((params_t *) params)->n_reactions;
  int n_species = ((params_t *) params)->n_species;
  double nh = ((params_t *) params)->nh;
  double av = ((params_t *) params)->av;
  double chi = ((params_t *) params)->chi;
  double grain_size = ((params_t *) params)->grain_size;
  double grain_abundance = ((params_t *) params)->grain_abundance;

  /* Compute the jacobian matrix. */

  for (i = 0; i < n_species; i++)
    {
      for (j = 0; j < n_species; j++)
        {
          DENSE_ELEM (J, i, j) = 0;
        }
    }

  for (i = 0; i < n_reactions; i++)
    {

      /* Compute the Jacobian matrix elements corresponding to
         the reaction. */

      double y_product;

      if (reactions[i].reaction_type == 0)
        {
          /* H2 formation on grains */

          DENSE_ELEM (J, reactions[i].reactant1, reactions[i].reactant1) -=
           2 * reac_rates[i];
          DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant1) +=
           reac_rates[i];
        }
      else if (reactions[i].reaction_type == 23)
        {
          /* Photo-desorption */

          double jac_elem;

          jac_elem =
           (chi * AVERAGE_UV_IRSF * exp (-2 * av) * reactions[i].alpha) /
           (GRAIN_SITES_PER_CM2 * reactions[i].gamma) *
           exp (-NV_Ith_S (y, reactions[i].reactant1) /
                (GRAIN_SITES_PER_CM2 * M_PI * pow (grain_size, 2) *
                 grain_abundance * nh * reactions[i].gamma));

          DENSE_ELEM (J, reactions[i].reactant1, reactions[i].reactant1) -=
           jac_elem;
          DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant1) +=
           jac_elem;
        }
      else
        {
          /* Other reactions */

          if ((reactions[i].reactant1 != -1) &&
              (reactions[i].reactant2 == -1) &&
              (reactions[i].reactant3 == -1))
            {
              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant1) -= reac_rates[i];
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant1) +=
               reac_rates[i];
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant1) += reac_rates[i];
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant1) += reac_rates[i];
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant1) += reac_rates[i];
            }

          if ((reactions[i].reactant1 != -1) &&
              (reactions[i].reactant2 != -1) &&
              (reactions[i].reactant3 == -1))
            {
              y_product =
               reac_rates[i] * NV_Ith_S (y, reactions[i].reactant2);

              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant1) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant2,
                          reactions[i].reactant1) -= y_product;
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant1) +=
               y_product;
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant1) += y_product;
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant1) += y_product;
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant1) += y_product;

              y_product =
               reac_rates[i] * NV_Ith_S (y, reactions[i].reactant1);

              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant2) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant2,
                          reactions[i].reactant2) -= y_product;
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant2) +=
               y_product;
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant2) += y_product;
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant2) += y_product;
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant2) += y_product;
            }

          if ((reactions[i].reactant1 != -1) &&
              (reactions[i].reactant2 != -1) &&
              (reactions[i].reactant3 != -1))
            {
              y_product = reac_rates[i] * NV_Ith_S (y, reactions[i].reactant2)
               * NV_Ith_S (y, reactions[i].reactant3);

              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant1) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant2,
                          reactions[i].reactant1) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant3,
                          reactions[i].reactant1) -= y_product;
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant1) +=
               y_product;
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant1) += y_product;
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant1) += y_product;
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant1) += y_product;

              y_product = reac_rates[i] * NV_Ith_S (y, reactions[i].reactant1)
               * NV_Ith_S (y, reactions[i].reactant3);

              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant2) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant2,
                          reactions[i].reactant2) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant3,
                          reactions[i].reactant2) -= y_product;
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant2) +=
               y_product;
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant2) += y_product;
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant2) += y_product;
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant2) += y_product;

              y_product = reac_rates[i] * NV_Ith_S (y, reactions[i].reactant1)
               * NV_Ith_S (y, reactions[i].reactant2);

              DENSE_ELEM (J, reactions[i].reactant1,
                          reactions[i].reactant3) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant2,
                          reactions[i].reactant3) -= y_product;
              DENSE_ELEM (J, reactions[i].reactant3,
                          reactions[i].reactant3) -= y_product;
              DENSE_ELEM (J, reactions[i].product1, reactions[i].reactant3) +=
               y_product;
              if (reactions[i].product2 != -1)
                DENSE_ELEM (J, reactions[i].product2,
                            reactions[i].reactant3) += y_product;
              if (reactions[i].product3 != -1)
                DENSE_ELEM (J, reactions[i].product3,
                            reactions[i].reactant3) += y_product;
              if (reactions[i].product4 != -1)
                DENSE_ELEM (J, reactions[i].product4,
                            reactions[i].reactant3) += y_product;
            }
        }
    }

  return (0);
}

/**
 * @brief Solve the ODE system.
 * Solve the ODE system with fully provided parameters. It initialize and close the solver itself
 * @param cell_index index of cell to compute on
 * @param input_params input params
 * @param mode source mode
 * @param cell cell to compute on
 * @param network network to use for solving
 * @param ts time steps to solve on
 * @param[out] results output results
 * @param verbose quit if 0, verbose if 1
 * @return 0 if sucessfull
 * @todo Remove complicated code ? TODO
 */
int
full_solve (int cell_index, const inp_t * input_params, SOURCE_MODE mode,
            const cell_table_t * cell, const net_t * network, const time_steps_t * ts,
            res_t * results, int verbose)
{

  double *abundances = NULL;
  alloc_abundances( network, &abundances ); // Allocate the abundances array; it contains all species.

#if 0 //Ultra complicated code
  const species_name_t* species = malloc( input_params->abundances.n_initial_abundances * sizeof(*species));
  double *initial_abundances = malloc( input_params->abundances.n_initial_abundances * sizeof(double) );

  int i;
  for( i = 0; i <  input_params->abundances.n_initial_abundances ; i++ )
    {
      strcpy( network->species_names[input_params->abundances.initial_abundances[i].species_idx ] , species[i] );
      initial_abundances[i] = input_params->abundances.initial_abundances[i].abundance;
    }
  set_initial_abundances( species, 3, initial_abundances, &network, abundances); // Set initial abundances
#else // same thing , without using api
  int i;
  for( i = 0; i <  input_params->abundances.n_initial_abundances ; i++ )
    {
      abundances[ input_params->abundances.initial_abundances[i].species_idx ] = input_params->abundances.initial_abundances[i].abundance;
    }
#endif


  double min_nh;                 /* Minimum density */

  /* Compute the minimum density to set the absolute tolerance of the
     solver */
  min_nh = cell->nh[0];
  if (mode == DYNAMIC)
    {
      int i;

      for (i = 1; i < ts->n_time_steps; i++)
        {
          if (cell->nh[i] < min_nh)
            {
              min_nh = cell->nh[i];
            }
        }
    }

  astrochem_mem_t astrochem_mem;
  cell_t cell_unik;
  cell_unik.av = cell->av[0];
  cell_unik.nh = cell->nh[0];
  cell_unik.tgas = cell->tgas[0];
  cell_unik.tdust = cell->tdust[0];
  solver_init( &cell_unik, network, &input_params->phys, abundances, min_nh, input_params->solver.abs_err,  input_params->solver.rel_err, &astrochem_mem );

    {
      int i, j;

      /* Solve the system for each time step. */
      for (i = 0; i < ts->n_time_steps; i++)
        {


          if (i!=0 && mode == DYNAMIC)
            {
              cell_unik.av = cell->av[i];
              cell_unik.nh = cell->nh[i];
              cell_unik.tgas = cell->tgas[i];
              cell_unik.tdust = cell->tdust[i];

              solve( &astrochem_mem, network, abundances,  ts->time_steps[i], &cell_unik, verbose );
            }
          else
            {
              solve( &astrochem_mem, network, abundances,  ts->time_steps[i], NULL, verbose );
            }


          /* Fill the array of abundances with the output species
             abundances. Ignore species that are not in the
             network. Abundance that are lower than MIN_ABUNDANCES are
             set to 0. */

          for (j = 0; j < input_params->output.n_output_species; j++)
            {
              int idx = get_abundance_idx (results, cell_index, i, j);
              if (mode == STATIC)
                {
                  results->abundances[idx] =
                   (double) NV_Ith_S (astrochem_mem.y, input_params->output.output_species_idx[j]) / cell->nh[0];
                }
              else
                {
                  results->abundances[idx] =
                   (double) NV_Ith_S (astrochem_mem.y, input_params->output.output_species_idx[j]) / cell->nh[i];
                }
              if (results->abundances[idx] < MIN_ABUNDANCE)
                results->abundances[idx] = 0.;
            }

          /* Compute the rate of each formation/destruction route for
             each output specie. */

          if (input_params->output.trace_routes)
            {
              for (j = 0; j < input_params->output.n_output_species; j++)
                {
                  int k;
                  int l;
                  for (l = 0; l < N_OUTPUT_ROUTES; l++)
                    {
                      int idx = get_route_idx (results, cell_index, i, j, l);
                      results->routes[idx].formation.rate = 0.;
                      results->routes[idx].destruction.rate = 0.;
                    }
                  for (k = 0; k < network->n_reactions; k++)
                    {
                      /* If the species is a product of the
                         reaction then compute the formation
                         rate. If the rate is greater than the
                         smallest rate in the formation route
                         structure, we add the current reaction
                         number and rate to that structure. */

                      if ((network->reactions[k].product1 ==
                           input_params->output.output_species_idx[j])
                          || (network->reactions[k].product2 ==
                              input_params->output.output_species_idx[j])
                          || (network->reactions[k].product3 ==
                              input_params->output.output_species_idx[j])
                          || (network->reactions[k].product4 ==
                              input_params->output.output_species_idx[j]))
                        {
                          r_t formation_route;
                          double min_rate;
                          unsigned int min_rate_index;
                          if (network->reactions[k].reaction_type == 0)
                            {
                              formation_route.rate = astrochem_mem.params.reac_rates[k];
                              formation_route.rate *=
                               NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant1);
                            }
                          else if (network->reactions[k].reaction_type == 23)
                            {
                              formation_route.rate = astrochem_mem.params.reac_rates[k];
                            }
                          else
                            {
                              formation_route.rate = astrochem_mem.params.reac_rates[k];
                              formation_route.rate *=
                               NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant1);
                              if (network->reactions[k].reactant2 != -1)
                                formation_route.rate *=
                                 NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant2);
                              if (network->reactions[k].reactant3 != -1)
                                formation_route.rate *=
                                 NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant3);
                            }
                          formation_route.reaction_no =
                           network->reactions[k].reaction_no;
                          min_rate =
                           results->routes[get_route_idx
                           (results, cell_index, i, j,
                            0)].formation.rate;
                          min_rate_index = 0;
                          for (l = 1; l < N_OUTPUT_ROUTES; l++)
                            {
                              int idx =
                               get_route_idx (results, cell_index, i, j, l);
                              if (results->routes[idx].formation.rate <
                                  min_rate)
                                {
                                  min_rate =
                                   results->routes[idx].formation.rate;
                                  min_rate_index = (unsigned int) l;
                                }
                            }
                          if (formation_route.rate > min_rate)
                            {
                              int idx =
                               get_route_idx (results, cell_index, i, j,
                                              min_rate_index);
                              results->routes[idx].formation.rate =
                               formation_route.rate;
                              results->routes[idx].formation.reaction_no =
                               formation_route.reaction_no;
                            }
                        }

                      /* If the species is reactant of the reaction
                         then compute the destruction rate. */

                      if ((network->reactions[k].reactant1 ==
                           input_params->output.output_species_idx[j])
                          || (network->reactions[k].reactant2 ==
                              input_params->output.output_species_idx[j])
                          || (network->reactions[k].reactant3 ==
                              input_params->output.output_species_idx[j]))
                        {
                          r_t destruction_route;
                          double min_rate;
                          unsigned int min_rate_index;

                          if (network->reactions[k].reaction_type == 0)
                            {
                              destruction_route.rate = astrochem_mem.params.reac_rates[k];
                              destruction_route.rate *=
                               NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant1);
                            }
                          else if (network->reactions[k].reaction_type == 23)
                            {
                              destruction_route.rate = astrochem_mem.params.reac_rates[k];
                            }
                          else
                            {
                              destruction_route.rate = astrochem_mem.params.reac_rates[k];
                              if (network->reactions[k].reactant1 != -1)
                                destruction_route.rate *=
                                 NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant1);
                              if (network->reactions[k].reactant2 != -1)
                                destruction_route.rate *=
                                 NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant2);
                              if (network->reactions[k].reactant3 != -1)
                                destruction_route.rate *=
                                 NV_Ith_S (astrochem_mem.y, network->reactions[k].reactant3);
                            }
                          destruction_route.reaction_no =
                           network->reactions[k].reaction_no;

                          min_rate =
                           results->routes[get_route_idx
                           (results, cell_index, i, j,
                            0)].destruction.rate;
                          min_rate_index = 0;
                          for (l = 1; l < N_OUTPUT_ROUTES; l++)
                            {
                              int idx =
                               get_route_idx (results, cell_index, i, j, l);
                              if (results->routes[idx].destruction.rate <
                                  min_rate)
                                {
                                  min_rate =
                                   results->routes[idx].destruction.rate;
                                  min_rate_index = (unsigned int) l;
                                }
                            }
                          if (destruction_route.rate > min_rate)
                            {
                              int idx =
                               get_route_idx (results, cell_index, i, j,
                                              min_rate_index);
                              results->routes[idx].destruction.rate =
                               destruction_route.rate;
                              results->routes[idx].destruction.reaction_no =
                               destruction_route.reaction_no;
                            }
                        }
                    }
                }
            }
        }
    }
  solver_close( &astrochem_mem );
  return (0);
}

/**
 * @brief Initialize the solver
 * Initialize the solver before calling solve method. Do not forget to clse the solver after using solver_close
 *
 * @param cell cell to compute on
 * @param network network to compute on
 * @param phys physocs parameters
 * @param abundances initial abundances
 * @param density system density
 * @param abs_err absolute error
 * @param rel_err relative error
 * @param[out] astrochem_mem dedicated memory
 * return 0 if successfull
 */
int solver_init( const cell_t* cell, const net_t* network, const phys_t* phys,
                 const double* abundances , double density, double abs_err, double rel_err,
                 astrochem_mem_t* astrochem_mem )
{
  astrochem_mem->density = density;

  /* Allocate the work array and fill it with the initial
     concentrations. Ignore species that are not in the
     network. */

  if (( astrochem_mem->y = N_VNew_Serial (network->n_species)) == NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: %s\n", __FILE__, __LINE__,
               "array allocation failed.\n");
      return -1;
    }
  int i;
  for (i = 0; i < network->n_species; i++)
    {
      NV_Ith_S ( astrochem_mem->y, i) = abundances[i] * density;
    }

  /* Allocate an array for the reaction rates and compute them. */

  if ((astrochem_mem->params.reac_rates =
       malloc (sizeof (double) * (unsigned int) network->n_reactions)) ==
      NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: array allocation failed.\n",
               __FILE__, __LINE__);
      return -1;
    }
  for (i = 0; i < network->n_reactions; i++)
    {
      astrochem_mem->params.reac_rates[i] = rate (network->reactions[i].alpha,
                                                  network->reactions[i].beta,
                                                  network->reactions[i].gamma,
                                                  network->reactions[i].reaction_type,
                                                  network->reactions[i].reaction_no,
                                                  cell->nh, cell->av, cell->tgas,
                                                  cell->tdust, phys->chi,
                                                  phys->cosmic,
                                                  phys->grain_size,
                                                  phys->grain_abundance, 0.);
    }

  /* Fill out a structure containing the parameters of the function
     defining the ODE system and the jacobian. */

  astrochem_mem->params.reactions = network->reactions;
  astrochem_mem->params.n_reactions = network->n_reactions;
  astrochem_mem->params.n_species = network->n_species;
  astrochem_mem->params.nh = cell->nh;
  astrochem_mem->params.av = cell->av;
  astrochem_mem->params.tgas = cell->tgas;
  astrochem_mem->params.tdust = cell->tdust;
  astrochem_mem->params.chi = phys->chi;
  astrochem_mem->params.cosmic = phys->cosmic;
  astrochem_mem->params.grain_size = phys->grain_size;
  astrochem_mem->params.grain_abundance = phys->grain_abundance;

  /* Define the ODE system and solve it using the Backward
     Differential Formulae method (BDF) with a Newton Iteration. The
     absolute error is multiplied by the density, because we compute
     concentrations and not abundances. */

  if (( astrochem_mem->cvode_mem = CVodeCreate (CV_BDF, CV_NEWTON)) == NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: solver memory allocation failed.\n",
               __FILE__, __LINE__);
      return -1;
    }

  if ((CVodeInit ( astrochem_mem->cvode_mem, f, 0.0,  astrochem_mem->y ) != CV_SUCCESS)
      ||
      (CVodeSStolerances
       ( astrochem_mem->cvode_mem, rel_err,
         abs_err * density ) != CV_SUCCESS)
#ifdef USE_LAPACK
      || ((CVLapackDense ( astrochem_mem->cvode_mem, network->n_species) != CV_SUCCESS))
#else
      || ((CVDense ( astrochem_mem->cvode_mem, network->n_species) != CV_SUCCESS))
#endif
      || ((CVDlsSetDenseJacFn (  astrochem_mem->cvode_mem, jacobian) != CV_SUCCESS))
      || (CVodeSetUserData ( astrochem_mem->cvode_mem, &astrochem_mem->params) != CV_SUCCESS))
    {
      fprintf (stderr, "astrochem: %s:%d: solver initialization failed.\n",
               __FILE__, __LINE__);
      return -1;
    }
  return 0;
}

/**
 * @brief Solve the ODE system
 * @param astrochem_mem dedicated memory
 * @param network network to solve the system with
 * @param[out] abundances  output abundances
 * @param time time to compute on
 * @param new_cell new cell to compute on
 * @param verbose quiet if 0, verbose if 1
 * @return 0 if successfull
 */
int solve( astrochem_mem_t* astrochem_mem, const net_t* network, double* abundances, double time , const cell_t* new_cell, int verbose )
{

  /* Computing new reac rate and abundance if cell parameter have evolved
     since last call */
  if( new_cell != NULL )
    {
      int i,j;
      for (j = 0; j < network->n_species; j++)
        {
          NV_Ith_S (astrochem_mem->y, j) *= new_cell->nh / astrochem_mem->params.nh;
        }

      for (i = 0; i < network->n_reactions; i++)
        {
          astrochem_mem->params.reac_rates[i] = rate (network->reactions[i].alpha,
                                                      network->reactions[i].beta,
                                                      network->reactions[i].gamma,
                                                      network->reactions[i].reaction_type,
                                                      network->reactions[i].reaction_no,
                                                      new_cell->nh, new_cell->av, new_cell->tgas,
                                                      new_cell->tdust, astrochem_mem->params.chi,
                                                      astrochem_mem->params.cosmic,
                                                      astrochem_mem->params.grain_size,
                                                      astrochem_mem->params.grain_abundance, 0.);
        }

      /* Fill out a structure containing the parameters of the function
         defining the ODE system and the jacobian. */

      astrochem_mem->params.nh = new_cell->nh;
      astrochem_mem->params.av = new_cell->av;
      astrochem_mem->params.tgas = new_cell->tgas;
      astrochem_mem->params.tdust = new_cell->tdust;
    }

  realtype t = 0.0;
  CVode ( astrochem_mem->cvode_mem, (realtype) time, astrochem_mem->y, &t, CV_NORMAL);

  /* Print the cell number, time and time step after each call. */

  if (verbose >= 2)
    {
      realtype h;

      CVodeGetLastStep (astrochem_mem->cvode_mem, &h);
      fprintf (stdout, "t = %8.2e  delta_t = %8.2e\n",
               (double) t / CONST_MKSA_YEAR,
               (double) h / CONST_MKSA_YEAR);
    }
  int i;
  for( i = 0; i < network->n_species; i++ )
    {
      abundances[i] =  (double) NV_Ith_S ( astrochem_mem->y , i ) / astrochem_mem->density ;
    }
  return 0;
}

/**
 * @brief Close the solver
 * close the solver and desallocate dedicated memory, to be used after solver_init and solve functions
 *
 * @param astrochem_mem memory to deallocate
 */
void solver_close( astrochem_mem_t* astrochem_mem )
{
  if( astrochem_mem->params.reac_rates != NULL )
    {
      free (astrochem_mem->params.reac_rates);
    }
  if( astrochem_mem->y != NULL )
    {
      N_VDestroy_Serial (astrochem_mem->y);
    }
  if( astrochem_mem->cvode_mem != NULL )
    {
      CVodeFree (&astrochem_mem->cvode_mem);
    }
}

/**
 * @brief Allocate a abundances array using provided network
 *
 * Allocate an abundances array in prodided pointer to @p abundances array, 
 * of the size of n_species in @p network struct
 *
 * @param network network to use n_species from to allocate correct size
 * @param abundances pointer to array to allocate
 * @return 0 if successfull
 */
int alloc_abundances ( const net_t* network, double** abundances )
{
  if (( *abundances =
        malloc (sizeof (double) * network->n_species )) == NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: array allocation failed.\n",
               __FILE__, __LINE__);
      return -1;
    }
  int i;
  for( i = 0 ; i < network->n_species ; i++ )
    {
      (*abundances)[i]=0;
    }
  return 0;
}

/**
 * @brief Initialise specific abundances
 * @param species names of species
 * @param n_initialized_species number of species to initialize
 * @param initial_abundances intial abundances values
 * @param network network to use to find species names
 * @param[out] abundances abundances array to initalize
 * @return 0 if successfull
 */
int set_initial_abundances( const char** species, int n_initialized_species, const double* initial_abundances,
                            const net_t* network, double* abundances )
{
  int i,j;
  for( i = 0 ; i < network->n_alloc_species ; i++ )
    {
      for( j = 0 ; j < n_initialized_species ; j++ )
        {
          if( strcmp( network->species_names[i] , species[j] ) == 0 )
            {
              abundances[i] = initial_abundances[j];
            }
        }
    }
  return 0;
}

/**
 * @brief Free the abundances array
 * @param abundances abundances array to free
 */
void free_abundances ( double* abundances )
{
  if( abundances != NULL )
    {
      free ( abundances);
    }
}


/**
 * @brief Allocate the results structures
 * @param[out] results results to allocate
 * @param n_time_steps number of time steps
 * @param n_cells number of cells
 * @param n_output_abundances number of output abundances
 */
void
alloc_results (res_t * results, int n_time_steps, int n_cells,
               int n_output_abundances)
{
  int i;
  if ((results->abundances =
       malloc (sizeof (double) * n_cells * n_time_steps *
               n_output_abundances)) == NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: array allocation failed.\n",
               __FILE__, __LINE__);
      exit (1);
    }
  for(i=0;i<n_cells * n_time_steps * n_output_abundances; i++)
    {
      results->abundances[i]=0;
    }
  if ((results->routes =
       malloc (sizeof (rout_t) * n_cells * n_time_steps *
               n_output_abundances * N_OUTPUT_ROUTES)) == NULL)
    {
      fprintf (stderr, "astrochem: %s:%d: array allocation failed.\n",
               __FILE__, __LINE__);
      exit (1);
    }
  for(i=0;i<n_cells * n_time_steps * n_output_abundances * N_OUTPUT_ROUTES; i++)
    {
      results->routes[i].formation.reaction_no=0;
      results->routes[i].formation.rate=0;
      results->routes[i].destruction.reaction_no=0;
      results->routes[i].destruction.rate=0;
    }
  results->n_time_steps = n_time_steps;
  results->n_cells = n_cells;
  results->n_output_abundances = n_output_abundances;
}

/**
 * @brief Free the results structures
 * @param results results to free
 */
void
free_results (res_t * results)
{
  if( results->routes != NULL )
    {
      free (results->routes);
    }
  if( results->abundances != NULL )
    {
      free (results->abundances);
    }
}

/**
 * @brief Get index in abundances array in results abundance array
 * @param results results to search abundances idx in
 * @param cell_idx index of cell
 * @param ts_idx index of time step
 * @param abund_idx index of abundance in network
 * @return index of abundance in results
 */
int
get_abundance_idx (const res_t * results, int cell_idx, int ts_idx,
                   int abund_idx)
{
  return (cell_idx * (results->n_output_abundances * results->n_time_steps) +
          ts_idx * results->n_output_abundances + abund_idx);
}

/**
 * @brief Get index in route array in results
 * @param results results to search route idx in
 * @param cell_idx index of cell
 * @param ts_idx index of time step
 * @param abund_idx index of abundance in network
 * @param route_idx index of route
 * @return index of route in results
 */
int
get_route_idx (const res_t * results, int cell_idx, int ts_idx, int abund_idx,
               int route_idx)
{
  return (cell_idx *
          (N_OUTPUT_ROUTES * results->n_output_abundances *
           results->n_time_steps) +
          ts_idx * (N_OUTPUT_ROUTES * results->n_output_abundances) +
          abund_idx * N_OUTPUT_ROUTES + route_idx);
}
