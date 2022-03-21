// Copyright 2020, Jan Schmidt
// SPDX-License-Identifier: BSL-1.0
#ifndef __UKF_H__
#define __UKF_H__

#include "matrices.h"
#include "unscented.h"

typedef struct ukf_base ukf_base;
typedef struct ukf_measurement ukf_measurement;

/* Process function that propagates a state vector */
typedef bool (* ukf_process_fn)(const ukf_base *ukf, const double dt, const matrix2d *X_prior, matrix2d *X);

/* Process function that generates a measurement vector from a state vector */
typedef bool (* ukf_measurement_fn)(const ukf_base *ukf, const ukf_measurement *m, const matrix2d *x, matrix2d *z);

struct ukf_base {
  /* UT transform for the state / process */
  unscented_transform ut_X;

  int N_state; /* Dimension of the state vector */
  int N_cov; /* Dimension of the covariance matrix */

  /* State vector N_statex1 */
  matrix2d *x_prior;
  /* Covariance N_cov x N_cov */
  matrix2d *P_prior;

  /* Process noise N_cov x N_cov */
  matrix2d *Q;

  int num_sigmas; /* Number of sigma points for the UT */

  /* Estimated state vector N_statex1, generated by ukf_base_predict() */
  matrix2d *x;
  /* Estimated covariance N_cov x N_cov, generated by ukf_base_predict() */
  matrix2d *P;

  ukf_process_fn process_fn;

  /* Internal working space */
  matrix2d *sigmas; /* Sigma point state vectors for X during predict (N x num_sigmas) */
  matrix2d *X_tmp_prior; /* N_state x 1 */
  matrix2d *X_tmp; /* N_state x 1 */

  matrix2d *P_tmp; /* N_cov x N_cov */
};

struct ukf_measurement {
  /* UT transform for the measurement update */
  unscented_transform ut_Z;

  ukf_measurement_fn measurement_fn;

  int N_measurement; /* Dimension of the measurement vector */
  int N_cov; /* Dimension of the measurement covariance */

  /* measured values column vector Nx1 */
  matrix2d *z;
  /* measurement noise (NxN) */
  matrix2d *R;

  int num_sigmas; /* Number of sigma points for the UT */

  /* Measurement estimate from UT transform */
  matrix2d *Z_est;

  /* Measurement estimate covariance N_cov x N_cov */
  matrix2d *Pz;

  /* cross-variance Nx by Nz */
  matrix2d *Pxz;

  /* innovation from estimated measurement Nx1 */
  matrix2d *y;
  /* Kalman gain from last measurement u->N_cov x N */
  matrix2d *K;

  /* Internal working space */
  matrix2d *sigmas; /* Sigma point measurement vectors for Z during predict (N x num_sigmas) */
  matrix2d *Pz_tmp1; /* N x N workspace */
  matrix2d *Pz_tmp2; /* N x N workspace */
};

/* Initialise the UKF process. Takes ownership of the process noise Q if supplied.
 * N_state is the dimension of the state vector, N_cov is the dimension of the covariance
 * matrix (which may have a different dimension to the state vector) */
void ukf_base_init(ukf_base *u, int N_state, int N_cov, matrix2d *Q, ukf_process_fn process_fn,
  unscented_mean_fn mean_fn, unscented_residual_fn residual_fn, unscented_sum_fn sum_fn);
void ukf_base_clear(ukf_base *u);

/* Generate a state prediction from the prior state (in x_prior, P_prior) into the
 * x and P matrices. Must be run at least once before an update */
bool ukf_base_predict(ukf_base *u, double dt);
/* Prediction function that allows for a custom process fn to subsitute the
 * default one configured on the UKF. Useful for (for example) updating augmented
 * state variables */
bool ukf_base_predict_with_process(ukf_base *u, double dt, ukf_process_fn process_fn);
/* Use a measurement of the state to update the prior. ukf_base_predict() must be
 * called at least once before in order to generate the state estimate in the x/P
 * matrices */
bool ukf_base_update(ukf_base *u, ukf_measurement *m);
/* Copy the estimated X and covariance back to the x_prior/P_prior. Used when propagating
 * the filter without an observation measurement */
bool ukf_base_commit(ukf_base *u);

/* Initialise a ukf_measurement. N_measurement is the dimension of the measurement vector.
 * The number of sigma points and their weights are extracted from the ukf_base supplied */
void ukf_measurement_init(ukf_measurement *m, int N_measurement, int N_cov, const ukf_base *u, ukf_measurement_fn measurement_fn,
  unscented_mean_fn mean_fn, unscented_residual_fn residual_fn, unscented_sum_fn sum_fn);
void ukf_measurement_clear(ukf_measurement *m);
#endif
