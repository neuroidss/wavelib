/*
  Copyright (c) 2014, Rafat Hussain
*/

#include "wavelib.h"

wave_object wave_init(char* wname) {
	wave_object obj = NULL;
	int retval;
	retval = 0;

	if (wname != NULL) {
		retval = filtlength(wname);
		//obj->filtlength = retval;
		//strcopy(obj->wname, wname);
	}

	obj = (wave_object)malloc(sizeof(struct wave_set) + sizeof(double)* 4 * retval);

	obj->filtlength = retval;
	obj->lpd_len = obj->hpd_len = obj->lpr_len = obj->hpr_len = obj->filtlength;
	strcpy(obj->wname, wname);
	if (wname != NULL) {
		filtcoef(wname,obj->params,obj->params+retval,obj->params+2*retval,obj->params+3*retval);
	}
	obj->lpd = &obj->params[0];
	obj->hpd = &obj->params[retval];
	obj->lpr = &obj->params[2 * retval];
	obj->hpr = &obj->params[3 * retval];
	return obj;
}

wt_object wt_init(wave_object wave,char* method, int siglength,int J) {
	int size,i,MaxIter;
	wt_object obj = NULL;

	size = wave->filtlength;

	if (J > 100) {
		printf("\n The Decomposition Iterations Cannot Exceed 100. Exiting \n");
		exit(-1);
	}

	MaxIter = wmaxiter(siglength, size);

	if (J > MaxIter) {
		printf("\n Error - The Signal Can only be iterated %d times using this wavelet. Exiting\n",MaxIter);
		exit(-1);
	}

	if (method == NULL) {
		obj = (wt_object)malloc(sizeof(struct wt_set) + sizeof(double)* (siglength +  2 * J * (size+1)));
		obj->outlength = siglength + 2 * J * (size + 1); // Default
		strcpy(obj->ext, "sym"); // Default
	}
	else if (!strcmp(method, "dwt") || !strcmp(method, "DWT")) {
		obj = (wt_object)malloc(sizeof(struct wt_set) + sizeof(double)* (siglength + 2 * J * (size + 1)));
		obj->outlength = siglength + 2 * J * (size + 1); // Default
		strcpy(obj->ext, "sym"); // Default
	}
	else if (!strcmp(method, "swt") || !strcmp(method, "SWT")) {
		if (!testSWTlength(siglength, J)) {
			printf("\n For SWT the signal length must be a multiple of 2^J. \n");
			exit(-1);
		}

		obj = (wt_object)malloc(sizeof(struct wt_set) + sizeof(double)* (siglength * (J + 1)));
		obj->outlength = siglength * (J + 1); // Default
		strcpy(obj->ext, "per"); // Default
	}
	else if (!strcmp(method, "modwt") || !strcmp(method, "MODWT")) {

		if (!strstr(wave->wname,"db")) {
			if (!strstr(wave->wname, "sym")) {
				if (!strstr(wave->wname, "coif")) {
					printf("\n MODWT is only implemented for orthogonal wavelet families - db, sym and coif \n");
					exit(-1);
				}
			}
		}

		obj = (wt_object)malloc(sizeof(struct wt_set) + sizeof(double)* (siglength * (J + 1)));
		obj->outlength = siglength * (J + 1); // Default
		strcpy(obj->ext, "per"); // Default
	}

	obj->wave = wave;
	obj->siglength = siglength;
	obj->J = J;
	obj->MaxIter = MaxIter;
	strcpy(obj->method, method);

	if (siglength % 2 == 0) {
		obj->even = 1;
	}
	else {
		obj->even = 0;
	}

	obj->cobj = NULL;

	strcpy(obj->cmethod, "direct"); // Default
	obj->cfftset = 0;
	obj->lenlength = J + 2;
	obj->output = &obj->params[0];
	if (!strcmp(method, "dwt") || !strcmp(method, "DWT")) {
		for (i = 0; i < siglength + 2 * J * (size + 1); ++i) {
			obj->params[i] = 0.0;
		}
	}
	else if (!strcmp(method, "swt") || !strcmp(method, "SWT") || !strcmp(method, "modwt") || !strcmp(method, "MODWT")) {
		for (i = 0; i < siglength * (J + 1); ++i) {
			obj->params[i] = 0.0;
		}
	}
	//wave_summary(obj->wave);

	return obj;
}

static void wconv(wt_object wt, double *sig, int N, double *filt, int L, double *oup) {
	if (!strcmp(wt->cmethod,"direct")) {
		conv_direct(sig, N, filt, L, oup);
	}
	else if (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT")) {
		if (wt->cfftset == 0) {
			wt->cobj = conv_init(N, L);
			conv_fft(wt->cobj, sig, filt, oup);
			free_conv(wt->cobj);
		}
		else {
			conv_fft(wt->cobj, sig, filt, oup);
		}
	}
	else {
		printf("Convolution Only accepts two methods - direct and fft");
		exit(-1);
	}
}


static void dwt_per(wt_object wt, double *inp, int N, double *cA, int len_cA, double *cD, int len_cD) {
	int l,l2,isodd,i,t,len_avg;

	len_avg = wt->wave->lpd_len;
	l2 = len_avg / 2;
	isodd = N % 2;

	for (i = 0; i < len_cA; ++i) {
		t = 2 * i + l2;
		cA[i] = 0.0;
		cD[i] = 0.0;
		for (l = 0; l < len_avg; ++l) {
			if ((t - l) >= l2 && (t - l) < N) {
				cA[i] += wt->wave->lpd[l] * inp[t - l];
				cD[i] += wt->wave->hpd[l] * inp[t - l];
			}
			else if ((t - l) < l2 && (t - l) >= 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - l];
				cD[i] += wt->wave->hpd[l] * inp[t - l];
			}
			else if ((t - l) < 0 && isodd == 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - l + N];
				cD[i] += wt->wave->hpd[l] * inp[t - l + N];
			}
			else if ((t - l) < 0 && isodd == 1) {
				if ((t - l) != -1) {
					cA[i] += wt->wave->lpd[l] * inp[t - l + N + 1];
					cD[i] += wt->wave->hpd[l] * inp[t - l + N + 1];
				}
				else {
					cA[i] += wt->wave->lpd[l] * inp[N - 1];
					cD[i] += wt->wave->hpd[l] * inp[N - 1];
				}
			}
			else if ((t - l) >= N && isodd == 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - l - N];
				cD[i] += wt->wave->hpd[l] * inp[t - l - N];
			}
			else if ((t - l) >= N && isodd == 1) {
				if (t - l != N) {
					cA[i] += wt->wave->lpd[l] * inp[t - l - (N + 1)];
					cD[i] += wt->wave->hpd[l] * inp[t - l - (N + 1)];
				}
				else {
					cA[i] += wt->wave->lpd[l] * inp[N - 1];
					cD[i] += wt->wave->hpd[l] * inp[N - 1];
				}
			}

		}
	}


}

static void dwt_sym(wt_object wt, double *inp, int N, double *cA, int len_cA, double *cD, int len_cD) {
	int i,l, t, len_avg;

	len_avg = wt->wave->lpd_len;

	for (i = 0; i < len_cA; ++i) {
		t = 2 * i + 1;
		cA[i] = 0.0;
		cD[i] = 0.0;
		for (l = 0; l < len_avg; ++l) {
			if ((t - l) >= 0 && (t - l) < N) {
				cA[i] += wt->wave->lpd[l] * inp[t - l];
				cD[i] += wt->wave->hpd[l] * inp[t - l];
			}
			else if ((t - l) < 0) {
				cA[i] += wt->wave->lpd[l] * inp[-t + l - 1];
				cD[i] += wt->wave->hpd[l] * inp[-t + l - 1];
			}
			else if ((t - l) >= N) {
				cA[i] += wt->wave->lpd[l] * inp[2 * N - t + l - 1];
				cD[i] += wt->wave->hpd[l] * inp[2 * N - t + l - 1];
			}

		}
	}


}

static void dwt1(wt_object wt,double *sig,int len_sig, double *cA, int len_cA, double *cD, int len_cD) {
	int len_avg,D,lf;
	double *signal,*cA_undec;
	len_avg = (wt->wave->lpd_len + wt->wave->hpd_len) / 2;
	//len_sig = 2 * (int)ceil((double)len_sig / 2.0);

	D = 2;

	if (!strcmp(wt->ext, "per")) {
		signal = (double*)malloc(sizeof(double)* (len_sig + len_avg + (len_sig % 2)));

		len_sig = per_ext(sig, len_sig, len_avg / 2, signal);

		cA_undec = (double*)malloc(sizeof(double)* (len_sig + len_avg + wt->wave->lpd_len - 1));

		if (wt->wave->lpd_len == wt->wave->hpd_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
			wt->cobj = conv_init(len_sig + len_avg, wt->wave->lpd_len);
			wt->cfftset = 1;
		}
		else if (!(wt->wave->lpd_len == wt->wave->hpd_len)) {
			printf("Decomposition Filters must have the same length.");
			exit(-1);
		}

		wconv(wt, signal, len_sig + len_avg, wt->wave->lpd, wt->wave->lpd_len, cA_undec);

		downsamp(cA_undec + len_avg, len_sig, D, cA);

		wconv(wt, signal, len_sig + len_avg, wt->wave->hpd, wt->wave->hpd_len, cA_undec);

		downsamp(cA_undec + len_avg, len_sig, D, cD);
	}
	else if (!strcmp(wt->ext, "sym")) {
		//printf("\n YES %s \n", wt->ext);
		lf = wt->wave->lpd_len;// lpd and hpd have the same length

		signal = (double*)malloc(sizeof(double)* (len_sig + 2 * (lf - 1)));

		len_sig = symm_ext(sig, len_sig, lf - 1, signal);

		cA_undec = (double*)malloc(sizeof(double)* (len_sig + 3 * (lf - 1)));

		if (wt->wave->lpd_len == wt->wave->hpd_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
			wt->cobj = conv_init(len_sig + 2 * (lf - 1), lf);
			wt->cfftset = 1;
		}
		else if (!(wt->wave->lpd_len == wt->wave->hpd_len)) {
			printf("Decomposition Filters must have the same length.");
			exit(-1);
		}


		wconv(wt, signal, len_sig + 2 * (lf - 1), wt->wave->lpd, wt->wave->lpd_len, cA_undec);

		downsamp(cA_undec + lf, len_sig + lf - 2, D, cA);

		wconv(wt, signal, len_sig + 2 * (lf - 1), wt->wave->hpd, wt->wave->hpd_len, cA_undec);

		downsamp(cA_undec + lf, len_sig + lf - 2, D, cD);
	}
	else {
		printf("Signal extension can be either per or sym");
		exit(-1);
	}


	if (wt->wave->lpd_len == wt->wave->hpd_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		free_conv(wt->cobj);
		wt->cfftset = 0;
	}

	free(signal);
	free(cA_undec);
}

void dwt(wt_object wt,double *inp) {
	int i,J,temp_len,iter,N,lp;
	int len_cA;
	double *orig,*orig2;

	temp_len = wt->siglength;
	J = wt->J;
	wt->length[J + 1] = temp_len;
	wt->outlength = 0;
	wt->zpad = 0;
	orig = (double*)malloc(sizeof(double)* temp_len);
	orig2 = (double*)malloc(sizeof(double)* temp_len);
	/*
	if ((temp_len % 2) == 0) {
		wt->zpad = 0;
		orig = (double*)malloc(sizeof(double)* temp_len);
		orig2 = (double*)malloc(sizeof(double)* temp_len);
	}
	else {
		wt->zpad = 1;
		temp_len++;
		orig = (double*)malloc(sizeof(double)* temp_len);
		orig2 = (double*)malloc(sizeof(double)* temp_len);
	}
	*/

	for (i = 0; i < wt->siglength; ++i) {
		orig[i] = inp[i];
	}

	if (wt->zpad == 1) {
		orig[temp_len - 1] = orig[temp_len - 2];
	}

	N = temp_len;
	lp = wt->wave->lpd_len;

	if (!strcmp(wt->ext,"per")) {
		i = J;
		while (i > 0) {
			N = (int)ceil((double)N / 2.0);
			wt->length[i] = N;
			wt->outlength += wt->length[i];
			i--;
		}
		wt->length[0] = wt->length[1];
		wt->outlength += wt->length[0];
		N = wt->outlength;

		for (iter = 0; iter < J; ++iter) {
			len_cA = wt->length[J - iter];
			N -= len_cA;
			if ( !strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT") ) {
				dwt1(wt, orig, temp_len, orig2, len_cA, wt->params + N, len_cA);
			}
			else {
				dwt_per(wt, orig, temp_len, orig2, len_cA, wt->params + N, len_cA);
			}
			temp_len = wt->length[J - iter];
			if (iter == J - 1) {
				for (i = 0; i < len_cA; ++i) {
					wt->params[i] = orig2[i];
				}
			}
			else {
				for (i = 0; i < len_cA; ++i) {
					orig[i] = orig2[i];
				}
			}
		}
	}
	else if (!strcmp(wt->ext,"sym")) {
		//printf("\n YES %s \n", wt->ext);
		i = J;
		while (i > 0) {
			N = N + lp - 2;
			N = (int) ceil((double)N / 2.0);
			wt->length[i] = N;
			wt->outlength += wt->length[i];
			i--;
		}
		wt->length[0] = wt->length[1];
		wt->outlength += wt->length[0];
		N = wt->outlength;

		for (iter = 0; iter < J; ++iter) {
			len_cA = wt->length[J - iter];
			N -= len_cA;
			if (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT")) {
				dwt1(wt, orig, temp_len, orig2, len_cA, wt->params + N, len_cA);
			}
			else {
				dwt_sym(wt, orig, temp_len, orig2, len_cA, wt->params + N, len_cA);
			}
			temp_len = wt->length[J - iter];

			if (iter == J - 1) {
				for (i = 0; i < len_cA; ++i) {
					wt->params[i] = orig2[i];
				}
			}
			else {
				for (i = 0; i < len_cA; ++i) {
					orig[i] = orig2[i];
				}
			}
		}
	}
	else {
		printf("Signal extension can be either per or sym");
		exit(-1);
	}

	free(orig);
	free(orig2);
}

static void idwt1(wt_object wt,double *temp, double *cA_up,double *cA, int len_cA,double *cD,int len_cD,double *X_lp,double *X_hp,double *X) {
	int len_avg, N, U,N2,i;

	len_avg = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;
	N = 2 * len_cD;
	U = 2;

	upsamp2(cA, len_cA, U, cA_up);

	per_ext(cA_up, 2 * len_cA, len_avg / 2, temp);

	N2 = 2 * len_cA + len_avg;

	if (wt->wave->lpr_len == wt->wave->hpr_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		wt->cobj = conv_init(N2, len_avg);
		wt->cfftset = 1;
	}
	else if (!(wt->wave->lpr_len == wt->wave->hpr_len)) {
		printf("Decomposition Filters must have the same length.");
		exit(-1);
	}

	wconv(wt, temp, N2, wt->wave->lpr, len_avg, X_lp);

	upsamp2(cD, len_cD, U, cA_up);

	per_ext(cA_up, 2 * len_cD, len_avg / 2, temp);

	N2 = 2 * len_cD + len_avg;

	wconv(wt, temp, N2, wt->wave->hpr, len_avg, X_hp);


	for (i = len_avg - 1; i < N + len_avg - 1; ++i) {
		X[i - len_avg + 1] = X_lp[i] + X_hp[i];
	}

	if (wt->wave->lpr_len == wt->wave->hpr_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		free_conv(wt->cobj);
		wt->cfftset = 0;
	}

}

static void idwt_per(wt_object wt, double *cA, int len_cA, double *cD, int len_cD,  double *X) {
	int len_avg, N, i,l,m,n,t,v,l2;

	len_avg = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;
	N = 2 * len_cA;
	l2 = len_avg / 2;
	m = -2;
	n = -1;

	for (i = 0; i < len_cA + l2 - 1; ++i) {
		m += 2;
		n += 2;
		X[m] = 0.0;
		X[n] = 0.0;
		for (l = 0; l < l2; ++l) {
			t = 2 * l;
			if ((i - l) >= 0 && (i - l) < len_cA) {
				X[m] += wt->wave->lpr[t] * cA[i - l] + wt->wave->hpr[t] * cD[i - l];
				X[n] += wt->wave->lpr[t + 1] * cA[i - l] + wt->wave->hpr[t + 1] * cD[i - l];
			}
			else if ((i - l) >= len_cA && (i-l) < len_cA + len_avg - 1) {
				X[m] += wt->wave->lpr[t] * cA[i - l - len_cA] + wt->wave->hpr[t] * cD[i - l - len_cA];
				X[n] += wt->wave->lpr[t + 1] * cA[i - l - len_cA] + wt->wave->hpr[t + 1] * cD[i - l - len_cA];
			}
			else if ((i - l) < 0 && (i-l) > -l2) {
				X[m] += wt->wave->lpr[t] * cA[len_cA + i - l] + wt->wave->hpr[t] * cD[len_cA + i - l];
				X[n] += wt->wave->lpr[t + 1] * cA[len_cA + i - l] + wt->wave->hpr[t + 1] * cD[len_cA + i - l];
			}
		}
	}
}

static void idwt_sym(wt_object wt, double *cA, int len_cA, double *cD, int len_cD, double *X) {
	int len_avg, N, i, l, m, n, t, v,l2;

	len_avg = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;
	N = 2 * len_cA - 1;
	m = -2;
	n = -1;
	l2 = (len_avg - 2) / 2;

	for (v = 0; v < len_cA; ++v) {
		i = v;
		m += 2;
		n += 2;
		X[m] = 0.0;
		X[n] = 0.0;
		for (l = 0; l < len_avg / 2; ++l) {
			t = 2 * l;
			if ((i - l) >= 0 && (i - l) < len_cA) {
				X[m] += wt->wave->lpr[t] * cA[i - l] + wt->wave->hpr[t] * cD[i - l];
				X[n] += wt->wave->lpr[t + 1] * cA[i - l] + wt->wave->hpr[t + 1] * cD[i - l];
			}
		}
	}
}


void idwt(wt_object wt, double *dwtop) {
	int J,U,i,lf,N,N2,iter,k;
	int app_len, det_len;
	double *cA_up, *X_lp, *X_hp,*out,*temp;

	J = wt->J;
	U = 2;
	app_len = wt->length[0];
	out = (double*)malloc(sizeof(double)* (wt->siglength + 1));
	if (!strcmp(wt->ext, "per") && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		app_len = wt->length[0];
		det_len = wt->length[1];
		N = 2 * wt->length[J];
		lf = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;

		cA_up = (double*)malloc(sizeof(double)* N);
		temp = (double*)malloc(sizeof(double)* (N + lf));
		X_lp = (double*)malloc(sizeof(double)* (N + 2 * lf - 1));
		X_hp = (double*)malloc(sizeof(double)* (N + 2 * lf - 1));
		iter = app_len;

		for (i = 0; i < app_len; ++i) {
			out[i] = wt->output[i];
		}

		for (i = 0; i < J; ++i) {

			idwt1(wt,temp,cA_up,out,det_len,wt->output+iter,det_len,X_lp,X_hp,out);
			/*
			idwt_per(wt,out, det_len, wt->output + iter, det_len, X_lp);
			for (k = lf/2 - 1; k < 2 * det_len + lf/2 - 1; ++k) {
				out[k - lf/2 + 1] = X_lp[k];
			}
			*/
			iter += det_len;
			det_len = wt->length[i + 2];
		}
		free(cA_up);
		free(X_lp);
		free(X_hp);
		free(temp);

	}
	else if (!strcmp(wt->ext, "per") && !strcmp(wt->cmethod, "direct")) {
		app_len = wt->length[0];
		det_len = wt->length[1];
		N = 2 * wt->length[J];
		lf = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;

		X_lp = (double*)malloc(sizeof(double)* (N + 2 * lf - 1));
		iter = app_len;

		for (i = 0; i < app_len; ++i) {
			out[i] = wt->output[i];
		}

		for (i = 0; i < J; ++i) {

			//idwt1(wt, temp, cA_up, out, det_len, wt->output + iter, det_len, X_lp, X_hp, out);

			idwt_per(wt,out, det_len, wt->output + iter, det_len, X_lp);
			for (k = lf/2 - 1; k < 2 * det_len + lf/2 - 1; ++k) {
			out[k - lf/2 + 1] = X_lp[k];
			}

			iter += det_len;
			det_len = wt->length[i + 2];
		}

		free(X_lp);

	}
	else if (!strcmp(wt->ext, "sym") && !strcmp(wt->cmethod, "direct")) {
		app_len = wt->length[0];
		det_len = wt->length[1];
		N = 2 * wt->length[J] - 1;
		lf = (wt->wave->lpr_len + wt->wave->hpr_len) / 2;

		X_lp = (double*)malloc(sizeof(double)* (N + 2 * lf - 1));
		iter = app_len;

		for (i = 0; i < app_len; ++i) {
			out[i] = wt->output[i];
		}

		for (i = 0; i < J; ++i) {

			//idwt1(wt, temp, cA_up, out, det_len, wt->output + iter, det_len, X_lp, X_hp, out);

			idwt_sym(wt, out, det_len, wt->output + iter, det_len, X_lp);
			for (k = lf-2; k < 2 * det_len; ++k) {
				out[k - lf + 2] = X_lp[k];
			}

			iter += det_len;
			det_len = wt->length[i + 2];
		}

		free(X_lp);

	}
	else if (!strcmp(wt->ext, "sym") && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		lf = wt->wave->lpd_len;// lpd and hpd have the same length

		N = 2 * wt->length[J] - 1;
		cA_up = (double*)malloc(sizeof(double)* N);
		X_lp = (double*)malloc(sizeof(double)* (N + lf - 1));
		X_hp = (double*)malloc(sizeof(double)* (N + lf - 1));

		for (i = 0; i < app_len; ++i) {
			out[i] = wt->output[i];
		}

		iter = app_len;

		for (i = 0; i < J; ++i) {
			det_len = wt->length[i + 1];
			upsamp(out, det_len, U, cA_up);
			N2 = 2 * wt->length[i + 1] - 1;

			if (wt->wave->lpr_len == wt->wave->hpr_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
				wt->cobj = conv_init(N2, lf);
				wt->cfftset = 1;
			}
			else if (!(wt->wave->lpr_len == wt->wave->hpr_len)) {
				printf("Decomposition Filters must have the same length.");
				exit(-1);
			}

			wconv(wt, cA_up, N2, wt->wave->lpr, lf, X_lp);

			upsamp(wt->output + iter, det_len, U, cA_up);

			wconv(wt, cA_up, N2, wt->wave->hpr, lf, X_hp);

			for (k = lf - 2; k < N2 + 1; ++k) {
				out[k - lf + 2] = X_lp[k] + X_hp[k];
			}
			iter += det_len;
			if (wt->wave->lpr_len == wt->wave->hpr_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
				free_conv(wt->cobj);
				wt->cfftset = 0;
			}
		}

		free(cA_up);
		free(X_lp);
		free(X_hp);
	}
	else {
		printf("Signal extension can be either per or sym");
		exit(-1);
	}

	for (i = 0; i < wt->siglength; ++i) {
		dwtop[i] = out[i];
	}


	free(out);

}

static void swt_per(wt_object wt,int M, double *inp, int N, double *cA, int len_cA, double *cD, int len_cD) {
	int l, l2, isodd, i, t, len_avg,j;

	len_avg = M * wt->wave->lpd_len;
	l2 = len_avg / 2;
	isodd = N % 2;

	for (i = 0; i < len_cA; ++i) {
		t = i + l2;
		cA[i] = 0.0;
		cD[i] = 0.0;
		l = -1;
		for (j = 0; j < len_avg; j+=M) {
			l++;
			while (j >= len_cA) {
				j -= len_cA;
			}
			if ((t - j) >= l2 && (t -j) < N) {
				cA[i] += wt->wave->lpd[l] * inp[t - j];
				cD[i] += wt->wave->hpd[l] * inp[t - j];
			}
			else if ((t - j) < l2 && (t - j) >= 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - j];
				cD[i] += wt->wave->hpd[l] * inp[t - j];
			}
			else if ((t - j) < 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - j + N];
				cD[i] += wt->wave->hpd[l] * inp[t - j + N];
			}
			else if ((t - j) >= N && isodd == 0) {
				cA[i] += wt->wave->lpd[l] * inp[t - j - N];
				cD[i] += wt->wave->hpd[l] * inp[t - j - N];
			}
			else if ((t - j) >= N && isodd == 1) {
				if (t - l != N) {
					cA[i] += wt->wave->lpd[l] * inp[t - j - (N + 1)];
					cD[i] += wt->wave->hpd[l] * inp[t - j - (N + 1)];
				}
				else {
					cA[i] += wt->wave->lpd[l] * inp[N - 1];
					cD[i] += wt->wave->hpd[l] * inp[N - 1];
				}
			}

		}
	}


}

static void swt_fft(wt_object wt, double *inp) {
	int i, J, temp_len, iter, M, N, len_filt;
	int lenacc;
	double *low_pass, *high_pass,*sig,*cA,*cD;

	temp_len = wt->siglength;
	J = wt->J;
	wt->length[0] = wt->length[J] = temp_len;
	wt->outlength = wt->length[J+1] = (J + 1) * temp_len;
	M = 1;
	for (iter = 1; iter < J; ++iter) {
		M = 2 * M;
		wt->length[iter] = temp_len;
	}

	len_filt = wt->wave->filtlength;

	low_pass = (double*)malloc(sizeof(double)* M * len_filt);
	high_pass = (double*)malloc(sizeof(double)* M * len_filt);
	sig = (double*)malloc(sizeof(double)* (M * len_filt + temp_len + (temp_len%2)));
	cA = (double*)malloc(sizeof(double)* (2 * M * len_filt + temp_len + (temp_len % 2)) - 1);
	cD = (double*)malloc(sizeof(double)* (2 * M * len_filt + temp_len + (temp_len % 2)) - 1);

	M = 1;

	for (i = 0; i < temp_len; ++i) {
		wt->params[i] = inp[i];
	}

	lenacc = wt->outlength;

	for (iter = 0; iter < J; ++iter) {
		lenacc -= temp_len;
		if (iter > 0) {
			M = 2 * M;
			N = M * len_filt;
			upsamp2(wt->wave->lpd, wt->wave->lpd_len, M, low_pass);
			upsamp2(wt->wave->hpd, wt->wave->hpd_len, M, high_pass);
		}
		else {
			N = len_filt;
			for (i = 0; i < N; ++i) {
				low_pass[i] = wt->wave->lpd[i];
				high_pass[i] = wt->wave->hpd[i];
			}
		}

		//swt_per(wt,M, wt->params, temp_len, cA, temp_len, cD,temp_len);

		per_ext(wt->params, temp_len, N / 2, sig);

		if (wt->wave->lpd_len == wt->wave->hpd_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
			wt->cobj = conv_init(N + temp_len + (temp_len % 2), N);
			wt->cfftset = 1;
		}
		else if (!(wt->wave->lpd_len == wt->wave->hpd_len)) {
			printf("Decomposition Filters must have the same length.");
			exit(-1);
		}


		wconv(wt, sig, N + temp_len + (temp_len % 2), low_pass, N, cA);

		wconv(wt, sig, N + temp_len + (temp_len % 2), high_pass, N, cD);

		if (wt->wave->lpd_len == wt->wave->hpd_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
			free_conv(wt->cobj);
			wt->cfftset = 0;
		}

		for (i = 0; i < temp_len; ++i) {
			wt->params[i] = cA[N + i];
			wt->params[lenacc + i] = cD[N + i];
		}


	}

	free(low_pass);
	free(high_pass);
	free(sig);
	free(cA);
	free(cD);
}

static void swt_direct(wt_object wt, double *inp) {
	int i, J, temp_len, iter, M, N;
	int lenacc, len_filt;
	double  *cA, *cD;

	temp_len = wt->siglength;
	J = wt->J;
	wt->length[0] = wt->length[J] = temp_len;
	wt->outlength = wt->length[J + 1] = (J + 1) * temp_len;
	len_filt = wt->wave->filtlength;
	M = 1;
	for (iter = 1; iter < J; ++iter) {
		M = 2 * M;
		wt->length[iter] = temp_len;
	}


	cA = (double*)malloc(sizeof(double)* temp_len);
	cD = (double*)malloc(sizeof(double)* temp_len);

	M = 1;

	for (i = 0; i < temp_len; ++i) {
		wt->params[i] = inp[i];
	}

	lenacc = wt->outlength;

	for (iter = 0; iter < J; ++iter) {
		lenacc -= temp_len;
		if (iter > 0) {
			M = 2 * M;
			N = M * len_filt;
		}
		else {
			N = len_filt;
		}

		swt_per(wt, M, wt->params, temp_len, cA, temp_len, cD, temp_len);


		for (i = 0; i < temp_len; ++i) {
			wt->params[i] = cA[i];
			wt->params[lenacc + i] = cD[i];
		}

	}

	free(cA);
	free(cD);

}


void swt(wt_object wt, double *inp) {
	if (!strcmp(wt->method, "swt") && !strcmp(wt->cmethod, "direct") ) {
		swt_direct(wt,inp);
	}
	else if (!strcmp(wt->method, "swt") &&  (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
		swt_fft(wt, inp);
	}
	else {
		printf("SWT Only accepts two methods - direct and fft");
		exit(-1);
	}
}

void iswt(wt_object wt, double *swtop) {
	int N, lf, iter,i,J,index,value,count,len;
	int index_shift,len0,U,N1,N2,index2;
	double *low_pass, *high_pass;
	double *appx1, *det1,*appx_sig,*det_sig,*cL0,*cH0,*tempx,*oup00L,*oup00H,*oup00,*oup01,*appx2,*det2;

	N = wt->siglength;
	J = wt->J;
	U = 2;
	lf = wt->wave->lpr_len;

	appx_sig = (double*)malloc(sizeof(double)* N);
	det_sig = (double*)malloc(sizeof(double)* N);
	appx1 = (double*)malloc(sizeof(double)* N);
	det1 = (double*)malloc(sizeof(double)* N);
	appx2 = (double*)malloc(sizeof(double)* N);
	det2 = (double*)malloc(sizeof(double)* N);
	tempx = (double*)malloc(sizeof(double)* N);
	cL0 = (double*)malloc(sizeof(double)* (N + (N%2) + lf));
	cH0 = (double*)malloc(sizeof(double)* (N + (N % 2) + lf));
	oup00L = (double*)malloc(sizeof(double)* (N + 2 * lf));
	oup00H = (double*)malloc(sizeof(double)* (N + 2 * lf));
	oup00 = (double*)malloc(sizeof(double)* N);
	oup01 = (double*)malloc(sizeof(double)* N);



	for (iter = 0; iter < J; ++iter) {
		for (i = 0; i < N; ++i) {
			swtop[i] = 0.0;
		}
		if (iter == 0) {
			for (i = 0; i < N; ++i) {
				appx_sig[i] = wt->output[i];
				det_sig[i] = wt->output[N + i];
			}
		}
		else {
			for (i = 0; i < N; ++i) {
				det_sig[i] = wt->output[(iter + 1) * N + i];
			}
		}

		value = (int)pow(2.0, (double) (J - 1 - iter));

		for (count = 0; count < value; count++) {
			len = 0;
			for (index = count; index < N; index += value) {
				appx1[len] = appx_sig[index];
				det1[len] = det_sig[index];
				len++;
			}


			//SHIFT 0
			len0 = 0;

			for (index_shift = 0; index_shift < len; index_shift += 2) {
				appx2[len0] = appx1[index_shift];
				det2[len0] = det1[index_shift];
				len0++;
			}
			upsamp2(appx2, len0, U, tempx);
			per_ext(tempx, 2 * len0, lf / 2, cL0);

			upsamp2(det2, len0, U, tempx);
			per_ext(tempx, 2 * len0, lf / 2, cH0);

			N1 = 2 * len0 + lf;

			if (wt->wave->lpr_len == wt->wave->hpr_len && (!strcmp(wt->cmethod, "fft") || !strcmp(wt->cmethod, "FFT"))) {
				wt->cobj = conv_init(N1, lf);
				wt->cfftset = 1;
			}
			else if (!(wt->wave->lpd_len == wt->wave->hpd_len)) {
				printf("Decomposition Filters must have the same length.");
				exit(-1);
			}

			wconv(wt, cL0, N1, wt->wave->lpr, lf, oup00L);

			wconv(wt, cH0, N1, wt->wave->hpr, lf, oup00H);

			for (i = lf - 1; i < 2 * len0 + lf - 1; ++i) {
				oup00[i - lf + 1] = oup00L[i] + oup00H[i];
			}

			//SHIFT 1

			len0 = 0;

			for (index_shift = 1; index_shift < len; index_shift += 2) {
				appx2[len0] = appx1[index_shift];
				det2[len0] = det1[index_shift];
				len0++;
			}

			upsamp2(appx2, len0, U, tempx);
			per_ext(tempx, 2 * len0, lf / 2, cL0);

			upsamp2(det2, len0, U, tempx);
			per_ext(tempx, 2 * len0, lf / 2, cH0);

			N1 = 2 * len0 + lf;

			wconv(wt, cL0, N1, wt->wave->lpr, lf, oup00L);

			wconv(wt, cH0, N1, wt->wave->hpr, lf, oup00H);

			for (i = lf - 1; i < 2 * len0 + lf - 1; ++i) {
				oup01[i - lf + 1] = oup00L[i] + oup00H[i];
			}

			circshift(oup01, 2*len0, -1);

			index2 = 0;

			for (index = count; index < N; index += value) {
				swtop[index] = (oup00[index2] + oup01[index2]) / 2.0;
				index2++;
			}

		}
		for (i = 0; i < N; ++i) {
			appx_sig[i] = swtop[i];
		}

	}



	free(appx_sig);
	free(det_sig);
	free(appx1);
	free(det1);
	free(tempx);
	free(cL0);
	free(cH0);
	free(oup00L);
	free(oup00H);
	free(oup00);
	free(oup01);
	free(appx2);
	free(det2);
}

static void modwt_per(wt_object wt, int M, double *inp, int N, double *cA, int len_cA, double *cD, int len_cD) {
	int l, i, t, len_avg;
	double s;
	double *filt;
	len_avg = wt->wave->lpd_len;

	filt = (double*)malloc(sizeof(double)* 2 * len_avg);
	s = sqrt(2.0);
	for (i = 0; i < len_avg; ++i) {
		filt[i] = wt->wave->lpd[i] / s;
		filt[len_avg + i] = wt->wave->hpd[i] / s;
	}

	for (i = 0; i < len_cA; ++i) {
		t = i;
		cA[i] = filt[0] * inp[t];
		cD[i] = filt[len_avg] * inp[t];
		for (l = 1; l < len_avg; l++) {
			t -= M;
			while (t >= len_cA) {
				t -= len_cA;
			}
			while (t < 0) {
				t += len_cA;
			}

			cA[i] += filt[l] * inp[t];
			cD[i] += filt[len_avg + l] * inp[t];

		}
	}
	free(filt);
}

void modwt(wt_object wt, double *inp) {
	int i, J, temp_len, iter, M, N;
	int lenacc, len_filt;
	double  *cA, *cD;

	temp_len = wt->siglength;
	J = wt->J;
	wt->length[0] = wt->length[J] = temp_len;
	wt->outlength = wt->length[J + 1] = (J + 1) * temp_len;
	len_filt = wt->wave->filtlength;
	M = 1;
	for (iter = 1; iter < J; ++iter) {
		M = 2 * M;
		wt->length[iter] = temp_len;
	}


	cA = (double*)malloc(sizeof(double)* temp_len);
	cD = (double*)malloc(sizeof(double)* temp_len);

	M = 1;

	for (i = 0; i < temp_len; ++i) {
		wt->params[i] = inp[i];
	}

	lenacc = wt->outlength;

	for (iter = 0; iter < J; ++iter) {
		lenacc -= temp_len;
		if (iter > 0) {
			M = 2 * M;
			N = M * len_filt;
		}
		else {
			N = len_filt;
		}

		modwt_per(wt, M, wt->params, temp_len, cA, temp_len, cD, temp_len);


		for (i = 0; i < temp_len; ++i) {
			wt->params[i] = cA[i];
			wt->params[lenacc + i] = cD[i];
		}

	}

	free(cA);
	free(cD);

}

static void imodwt_per(wt_object wt,int M, double *cA, int len_cA, double *cD, int len_cD, double *X) {
	int len_avg, i, l, t, j;
	double s;
	double *filt;
	len_avg = wt->wave->lpd_len;

	filt = (double*)malloc(sizeof(double)* 2 * len_avg);
	s = sqrt(2.0);
	for (i = 0; i < len_avg; ++i) {
		filt[i] = wt->wave->lpd[i] / s;
		filt[len_avg + i] = wt->wave->hpd[i] / s;
	}


	for (i = 0; i < len_cA; ++i) {
		t = i;
		X[i] = (filt[0] * cA[t]) + (filt[len_avg] * cD[t]);
		for (l = 1; l < len_avg; l++) {
			t += M;
			while (t >= len_cA) {
				t -= len_cA;
			}
			while (t < 0) {
				t += len_cA;
			}

			X[i] += (filt[l] * cA[t]) + (filt[len_avg + l] * cD[t]);

		}
	}
	free(filt);
}

void imodwt(wt_object wt, double *dwtop) {
	int N, lf, iter, i, J, index, value, j, len,U;
	int lenacc,M;
	double *X;

	N = wt->siglength;
	J = wt->J;
	U = 2;
	lf = wt->wave->lpr_len;
	lenacc = N;
	M = (int)pow(2.0, (double)J - 1.0);
	//M = 1;
	X = (double*)malloc(sizeof(double)* N);

	for (i = 0; i < N; ++i) {
		dwtop[i] = wt->output[i];
	}

	for (iter = 0; iter < J; ++iter) {
		if (iter > 0) {
			M = M / 2;
		}
		imodwt_per(wt, M, dwtop, N, wt->params + lenacc, N, X);
		/*
		for (j = lf - 1; j < N; ++j) {
			dwtop[j - lf + 1] = X[j];
		}
		for (j = 0; j < lf - 1; ++j) {
			dwtop[N - lf + 1 + j] = X[j];
		}
		*/
		for (j = 0; j < N; ++j) {
			dwtop[j] = X[j];
		}

		lenacc += N;
	}
	free(X);
}

void setDWTExtension(wt_object wt, char *extension) {
	if (!strcmp(extension, "sym")) {
		strcpy(wt->ext, "sym");
	}
	else if (!strcmp(extension, "per")) {
		strcpy(wt->ext, "per");
	}
	else {
		printf("Signal extension can be either per or sym");
		exit(-1);
	}
}

void setWTConv(wt_object wt, char *cmethod) {
	if (!strcmp(cmethod, "fft") || !strcmp(cmethod, "FFT")) {
		strcpy(wt->cmethod, "fft");
	}
	else if (!strcmp(cmethod, "direct")) {
		strcpy(wt->cmethod, "direct");
	}
	else {
		printf("Convolution Only accepts two methods - direct and fft");
		exit(-1);
	}
}

void wave_summary(wave_object obj) {
	int i,N;
	N = obj->filtlength;
	printf("\n");
	printf("Wavelet Name : %s \n",obj->wname);
	printf("\n");
	printf("Wavelet Filters \n\n");
	printf("lpd : [");
	for (i = 0; i < N-1; ++i) {
		printf("%g,", obj->lpd[i]);
	}
	printf("%g", obj->lpd[N-1]);
	printf("] \n\n");
	printf("hpd : [");
	for (i = 0; i < N-1; ++i) {
		printf("%g,", obj->hpd[i]);
	}
	printf("%g", obj->hpd[N - 1]);
	printf("] \n\n");
	printf("lpr : [");
	for (i = 0; i < N-1; ++i) {
		printf("%g,", obj->lpr[i]);
	}
	printf("%g", obj->lpr[N - 1]);
	printf("] \n\n");
	printf("hpr : [");
	for (i = 0; i < N-1; ++i) {
		printf("%g,", obj->hpr[i]);
	}
	printf("%g", obj->hpr[N - 1]);
	printf("] \n\n");
}

void wt_summary(wt_object wt) {
	int i;
	int J,t;
	J = wt->J;
	wave_summary(wt->wave);
	printf("\n");
	printf("Wavelet Transform : %s \n", wt->method);
	printf("\n");
	printf("Signal Extension : %s \n", wt->ext);
	printf("\n");
	printf("Convolutional Method : %s \n", wt->cmethod);
	printf("\n");
	printf("Number of Decomposition Levels %d \n", wt->J);
	printf("\n");
	printf("Length of Input Signal %d \n", wt->siglength);
	printf("\n");
	printf("Length of WT Output Vector %d \n", wt->outlength);
	printf("\n");
	printf("Wavelet Coefficients are contained in vector : %s \n", "output");
	printf("\n");
	printf("Approximation Coefficients \n");
	printf("Level %d Access : output[%d] Length : %d \n", 1, 0, wt->length[0]);
	printf("\n");
	printf("Detail Coefficients \n");
	t = wt->length[0];
	for (i = 0; i < J; ++i) {
		printf("Level %d Access : output[%d] Length : %d \n", i + 1,t,wt->length[i+1]);
		t += wt->length[i+1];
	}
	printf("\n");

}

void wave_free(wave_object object) {
	free(object);
}

void wt_free(wt_object object) {
	free(object);
}
