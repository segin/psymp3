'DLL code for the FreeBasic compiler by V1ctor
' to compile  -->   fbc -dll fftlib.bas
'--------------------------------------------------------------------
'code from several sources:
' VB FFT Release 2-B by Murphy McCauley (MurphyMc@Concentric.NET) (removed that fft for errors)
' Stanford Meterology dept -- I think they derived their code from SETOOLS?

option explicit
option dynamic
'$INCLUDE: 'crt.bi'
'$INCLUDE: 'windows.bi'

'second protots
DECLARE FUNCTION CheckPowerOfTwo lib "fftlib" alias "CheckPowerOfTwo"(X As Long) AS INTEGER 
DECLARE FUNCTION ExactBlackman  lib "fftlib" alias "ExactBlackman"(n AS INTEGER, j AS INTEGER) AS SINGLE

DECLARE SUB FFTMagnitude lib "fftlib" alias "FFTMagnitude"(lpxr AS SINGLE PTR, lpyi AS SINGLE PTR, BYVAL n AS INTEGER, lpOutVal AS SINGLE PTR)
DECLARE SUB FFTPhase lib "fftlib" alias "FFTPhase"(lpxr AS SINGLE PTR, lpyi AS SINGLE PTR, BYVAL n AS INTEGER, lpPhase AS SINGLE PTR)
DECLARE FUNCTION Hamming lib "fftlib" alias "Hamming"(n AS INTEGER, j AS INTEGER) AS SINGLE
DECLARE FUNCTION Hanning lib "fftlib" alias "Hanning"(n AS INTEGER, j AS INTEGER) AS SINGLE
DECLARE FUNCTION Parzen lib "fftlib" alias "Parzen"(n AS INTEGER, j AS INTEGER) AS SINGLE
DECLARE FUNCTION SigmaGauss lib "fftlib" alias "SigmaGauss"(n AS INTEGER, j AS INTEGER) AS SINGLE
DECLARE FUNCTION SquareAndSum lib "fftlib" alias "SquareAndSum"(BYVAL a AS SINGLE, BYVAL b AS SINGLE) AS SINGLE
DECLARE FUNCTION Welch lib "fftlib" alias "Welch"(n AS INTEGER, j AS INTEGER) AS SINGLE
DECLARE SUB FFTFrequency lib "fftlib" alias "FFTFrequency"(lpHz AS SINGLE PTR, BYVAL Sample_Hz AS SINGLE, BYVAL n AS INTEGER)
DECLARE SUB Convolve lib "fftlib" alias "Convolve"(lpfiltcoef AS SINGLE PTR, lpIn AS SINGLE PTR, lpOut AS SINGLE PTR, BYVAL filtLen AS INTEGER, BYVAL aLen AS INTEGER)
DECLARE SUB DFT lib "fftlib" alias "DFT"(lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, BYVAL inverse AS INTEGER)
DECLARE SUB DFTMagnitude lib "fftlib" alias "DFTMagnitude"(lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, lpAmp AS SINGLE PTR)
DECLARE SUB DFTPhase lib "fftlib" alias "DFTPhase"(lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, lpPhase AS SINGLE PTR)
DECLARE SUB FFT2DCalc lib "fftlib" alias "FFT2DCalc"(lpxreal AS SINGLE PTR, lpyImag AS SINGLE PTR, BYVAL c AS INTEGER, BYVAL r AS INTEGER, BYVAL flag AS INTEGER)
DECLARE SUB FFT2DCRCalc (xreal() AS SINGLE, yimag() AS SINGLE, c AS INTEGER, r AS INTEGER, flag AS INTEGER) 
DECLARE SUB FFT2DRCCalc (xreal() AS SINGLE, yimag() AS SINGLE, c AS INTEGER, r AS INTEGER, flag AS INTEGER)

DECLARE SUB PowerSpectrumCalc lib "fftlib" alias "PowerSpectrumCalc"(lpxreal AS SINGLE PTR, lpyimag AS SINGLE PTR, BYVAL numdat AS INTEGER, BYVAL delta AS SINGLE)
DECLARE SUB RealFFT2 (xr() AS SINGLE, sinc() AS SINGLE, n AS INTEGER, inverse AS INTEGER)
DECLARE SUB WindowData lib "fftlib" alias "WindowData"(lpx AS SINGLE PTR, numdat AS INTEGER, wind AS INTEGER)
DECLARE SUB fft lib "fftlib" alias "fft"(lpxr AS SINGLE PTR, lpyi AS SINGLE PTR, BYVAL n AS INTEGER, BYVAL inverse AS INTEGER)
DECLARE SUB fft2 (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER)
DECLARE SUB reorder (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER, reel AS INTEGER)
DECLARE SUB rfft2 (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER)
DECLARE SUB trans (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, eval AS INTEGER)
DECLARE Sub CalcFrequency lib "fftlib" alias "CalcFrequency"(BYVAL NumberOfSamples As Long, BYVAL FrequencyIndex As Long, lpRealIn AS SINGLE PTR, lpImagIn AS SINGLE PTR, BYREF RealOut AS SINGLE, BYREF ImagOut AS SINGLE)

DECLARE Function NumberOfBitsNeeded(PowerOfTwo As Long) As Byte
DECLARE Function ReverseBits(ByVal Index As Long, NumBits As Byte) As Long



CONST fft.pi = 3.141592653589793#



' *** convolution of filtcoef (length filtLen) with input x()
' *** input x() of aLen => output y()
SUB Convolve (lpfiltcoef AS SINGLE PTR, lpIn AS SINGLE PTR, lpOut AS SINGLE PTR, BYVAL filtLen AS INTEGER, BYVAL aLen AS INTEGER) EXPORT
    DIM n AS INTEGER
    DIM m AS INTEGER
    DIM summ AS INTEGER
    DIM fp As Single Ptr:  fp = @lpfiltcoef : DIM filtcoef(filtLen) AS SINGLE : MEMCPY(@filtcoef(0), fp, filtLen * SIZEOF(SINGLE))
    DIM ip As Single Ptr:  ip = @lpIn:        DIM Inx(aLen) AS SINGLE : MEMCPY(@Inx(0), ip, aLen * SIZEOF(SINGLE))
    DIM op As Single Ptr:  op = @lpOut:       DIM Outy(aLen) AS SINGLE : MEMCPY(@Outy(0), op, aLen * SIZEOF(SINGLE))

      FOR n = 0 TO (aLen + filtLen - 2)
        summ = 0!
        FOR m = 0 TO filtLen - 1
    	IF ((n - m) >= 0) AND ((n - m) <= (aLen - 1)) THEN
    	  summ += filtcoef(m) * Inx(n - m)
    	END IF
        NEXT
        Outy(n) = summ
      NEXT
    MEMCPY(fp, @filtcoef(0), filtLen * SIZEOF(SINGLE))
    MEMCPY(ip, @Inx(0), aLen * SIZEOF(SINGLE))
    MEMCPY(op, @Outy(0), aLen * SIZEOF(SINGLE))
END SUB




'*********************************************************
'sub section of calculations for forward and inverse FFT
'*********************************************************
SUB fft2 (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER)

    DIM cc(0 TO m) AS INTEGER
    DIM indx AS INTEGER,  k0 AS INTEGER, k1 AS INTEGER, k2 AS INTEGER, k3 AS INTEGER
    DIM span AS INTEGER, j AS INTEGER, k AS INTEGER, kb AS INTEGER
    DIM kn AS INTEGER, fp AS INTEGER
    DIM mm AS INTEGER, mk AS INTEGER, breakf AS INTEGER, bf2 AS INTEGER 
    DIM rad AS SINGLE, c1 AS SINGLE, c2 AS SINGLE
    DIM c3 AS SINGLE, s1 AS SINGLE, s2 AS SINGLE, s3 AS SINGLE, ck AS SINGLE, sk AS SINGLE, sq
    DIM x0 AS SINGLE, x1 AS SINGLE, x2 AS SINGLE, x3 AS SINGLE
    DIM y0 AS SINGLE, y1 AS SINGLE, y2 AS SINGLE, y3

      sq = .707106781187#
      sk = .382683432366#
      ck = .92387953251#

      cc(m) = ks
      mm = (m \ 2) * 2
      kn = 0
      FOR k = m - 1 TO 0 STEP -1
	cc(k) = cc(k + 1) \ 2
      NEXT
      rad = 6.28318530718# / ((cc(0)) * (ks))
      mk = m - 5
      breakf = 0
      WHILE ((kn < n) AND (breakf = 0))

999     breakf = 0
	kb = kn
	kn = kn + ks

	IF (mm <> m) THEN
	  k2 = kn
	  k0 = cc(mm) + kb
	  WHILE (k0 > kb)
	    k2 = k2 - 1
	    k0 = k0 - 1
	    x0 = xr(k2)
	    y0 = y(k2)
	    xr(k2) = xr(k0) - x0
	    xr(k0) = xr(k0) + x0
	    y(k2) = y(k0) - y0
	    y(k0) = y(k0) + y0
	  WEND
	END IF

	c1 = 1!
	s1 = 0!
	indx = 0
	k = mm - 2
	j = 3

	IF (k >= 0) THEN
	   GOTO 1002
	ELSE
	   IF (kn < n) THEN
	     GOTO 999
	   ELSE
	     breakf = 1
	     GOTO 1008
	   END IF
	END IF
1001    bf2 = 1
	WHILE (bf2 = 1)
	  IF (cc(j) <= indx) THEN
	    indx = indx - cc(j)
	    j = j - 1
	    IF (cc(j) <= indx) THEN
	      indx = indx - cc(j)
	      j = j - 1
	      k = k + 2
	    END IF
	  ELSE
	    bf2 = 0
	  END IF
	WEND
	indx = cc(j) + indx
	j = 3

1002    span = cc(k)
	fp = 0
	IF (indx <> 0) THEN
	  c2 = (indx) * (span) * rad
	  c1 = COS(c2)
	  s1 = SIN(c2)
	END IF
1005    IF ((fp = 1) OR (indx <> 0)) THEN
	   c2 = c1 * c1 - s1 * s1
	   s2 = 2! * s1 * c1
	   c3 = c2 * c1 - s2 * s1
	   s3 = c2 * s1 + s2 * c1
	END IF
	FOR k0 = kb + span - 1 TO kb STEP -1
	  k1 = k0 + span
	  k2 = k1 + span
	  k3 = k2 + span
	  x0 = xr(k0)
	  y0 = y(k0)
	  IF (s1 = 0!) THEN
	    x1 = xr(k1)
	    y1 = y(k1)
	    x2 = xr(k2)
	    y2 = y(k2)
	    x3 = xr(k3)
	    y3 = y(k3)
	  ELSE
	    x1 = xr(k1) * c1 - y(k1) * s1
	    y1 = xr(k1) * s1 + y(k1) * c1
	    x2 = xr(k2) * c2 - y(k2) * s2
	    y2 = xr(k2) * s2 + y(k2) * c2
	    x3 = xr(k3) * c3 - y(k3) * s3
	    y3 = xr(k3) * s3 + y(k3) * c3
	  END IF
	  xr(k0) = x0 + x2 + x1 + x3
	  y(k0) = y0 + y2 + y1 + y3
	  xr(k1) = x0 + x2 - x1 - x3
	  y(k1) = y0 + y2 - y1 - y3
	  xr(k2) = x0 - x2 - y1 + y3
	  y(k2) = y0 - y2 + x1 - x3
	  xr(k3) = x0 - x2 + y1 - y3
	  y(k3) = y0 - y2 - x1 + x3
	NEXT
	IF (k > 0) THEN
	  k = k - 2
	  GOTO 1002
	END IF
	kb = k3 + span
	IF (kb < kn) THEN
	  IF (j = 0) THEN
	    k = 2
	    j = mk
	    GOTO 1001
	  END IF
	  j = j - 1
	  c2 = c1
	  IF (j = 1) THEN
	    c1 = c1 * ck + s1 * sk
	    s1 = s1 * ck - c2 * sk
	  ELSE
	    c1 = (c1 - s1) * sq
	    s1 = (s1 + c2) * sq
	  END IF
	  fp = 1
	  GOTO 1005
	END IF

1008  ' CONTINUE

      WEND

END SUB






'need all arrays to be a power of 2, except DFT(,,,)
FUNCTION CheckPowerOfTwo (X As Long) AS INTEGER EXPORT
    DIM chk as integer: chk = 0
    If (X And (X - 1)) = 0 Then chk = 1
    if x < 2 Then chk = 0
    CheckPowerOfTwo = chk
End FUNCTION




'============================================================================================
'THE DISCRETE FOURIER TRANSFORM  by JohnK, use this only for small data sets  
'code adapted from from: "The Scientist and Engineer's Guide to Digital Signal
'Processing (www.dspguide.com), which allows programs to be copied, distributed,
' and used for any noncommercial purpose. 
' *****   the signal in does not have to be a power of 2 ****
'    The frequency domain signals, held in Real and Imag components, are 
'    calculated from a time domain signal.
'    RawDat( )  holds the time domain signal
'    RealCom holds the real part of the frequency domain
'    Imag holds the imaginary part of the frequency domain
'
SUB DFT (lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, BYVAL inverse AS INTEGER) EXPORT
   DIM  Omega AS SINGLE:  Omega = 2.0! * fft.pi / Sample_N
   DIM k      AS INTEGER      'loops
   DIM i      AS INTEGER      'loops
   DIM Rsin   AS SINGLE
   DIM Isin   AS SINGLE       'complex sinusoids
   DIM RealSum(0 TO Sample_N) AS SINGLE
   DIM ImagSum(0 TO Sample_N) AS SINGLE
   DIM rp As Single Ptr:  rp = @lpReal : DIM Real(Sample_N) AS SINGLE : MEMCPY(@Real(0), rp, Sample_N * SIZEOF(SINGLE))
   DIM ip As Single Ptr:  ip = @lpImag : DIM Imag(Sample_N) AS SINGLE : MEMCPY(@Imag(0), ip, Sample_N * SIZEOF(SINGLE))


   IF inverse = 0 THEN        'THE FORWARD DISCRETE FOURIER TRANSFORM 

          FOR k = 0 TO (Sample_N/2)     'loops through each sample in Real and Imaginary component array
            FOR i = 0 TO Sample_N       'for all samples in output ( )
              Rsin = COS(Omega * k * i)           'speed issues
              Isin = -SIN(Omega * k * i)
              RealSum(k) += Real(i) * Rsin
              ImagSum(k) += Real(i) * Isin
              IF Imag(i) <> 0! THEN                             'don't waste computation for null array
                RealSum(k) += Imag(i) * Isin        'complex sinusoid
                ImagSum(k) += Imag(i) * Rsin
              END IF
            NEXT i
          NEXT k

    ELSE        'THE INVERSE DISCRETE FOURIER TRANSFORM  
        'The time domain signal, held in SigOut[ ] of size N, is calculated from the frequency 
        'domain signals, held in the real (real[ ]) and imaginary frequency domains (imag[ ])

        FOR k = 0 TO Sample_N/2     'Find the cosine and sine wave amplitudes 
            Real(k) =  Real(k) / (Sample_N/2)    
            Imag(k) = -Imag(k) / (Sample_N/2)  
        NEXT k                            
        Real(0) = Real(0) / 2
        Real(Sample_N/2) = Real(Sample_N/2) / 2


            'SYNTHESIS METHOD #1 Loop through each frequency generating the entire length of the sine & cosine 
            'waves, and add them to the accumulator OUTPUT signal
        FOR k = 0 TO Sample_N\2        'loops through each sample in Real( ) and Imag( )
            FOR i = 0 TO (Sample_N-1)  'loops through each sample in Output -- RealSum( )
                RealSum(i) += Real(k) * COS(Omega * k * i)
                RealSum(i) += Imag(k) * SIN(Omega * k * i)
            NEXT i
        NEXT k

   END IF       'FORWARD or INVERSE

  MEMCPY(rp, @RealSum(0), Sample_N/2 * SIZEOF(SINGLE))
  MEMCPY(ip, @ImagSum(0), Sample_N/2 * SIZEOF(SINGLE))
END SUB




SUB DFTMagnitude (lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, lpAmp AS SINGLE PTR) EXPORT
  DIM k      AS INTEGER      'loops
  DIM nm     AS SINGLE: nm = 2/Sample_N
  DIM rp As Single Ptr:  rp = @lpReal : DIM Real(Sample_N) AS SINGLE : MEMCPY(@Real(0), rp, Sample_N * SIZEOF(SINGLE))
  DIM ip As Single Ptr:  ip = @lpImag : DIM Imag(Sample_N) AS SINGLE : MEMCPY(@Imag(0), ip, Sample_N * SIZEOF(SINGLE))
  DIM ap As Single Ptr:  ap = @lpAmp  : DIM  Amp(Sample_N) AS SINGLE : MEMCPY(@Amp(0),  ap, Sample_N * SIZEOF(SINGLE))


  FOR k = 0 TO (Sample_N/2)
'    Amp(k) = 2 * SQR(Imag(k) * Imag(k) + Real(k) * Real(k))/ Sample_N
    Amp(k) = nm * SQR(Imag(k) * Imag(k) + Real(k) * Real(k))
  NEXT k
  MEMCPY(ap, @Amp(0), Sample_N * SIZEOF(SINGLE))

END SUB



SUB DFTPhase (lpReal AS SINGLE PTR, lpImag AS SINGLE PTR, BYVAL Sample_N AS INTEGER, lpPhase AS SINGLE PTR) EXPORT
  DIM k      AS INTEGER      'loops
  DIM rp As Single Ptr:  rp = @lpReal : DIM Real(Sample_N) AS SINGLE : MEMCPY(@Real(0), rp, Sample_N * SIZEOF(SINGLE))
  DIM ip As Single Ptr:  ip = @lpImag : DIM Imag(Sample_N) AS SINGLE : MEMCPY(@Imag(0), ip, Sample_N * SIZEOF(SINGLE))
  DIM pp As Single Ptr:  pp = @lpPhase :DIM Phase(Sample_N) AS SINGLE : MEMCPY(@Phase(0), pp, Sample_N * SIZEOF(SINGLE))

  DIM RealComp AS SINGLE   'store results for convenience of phase calc
  DIM ImagComp AS SINGLE


  FOR k = 0 TO (Sample_N/2)
    RealComp = Real(k)    'store results for convenience of phase calc
    ImagComp = Imag(k)
   'Compute the phase coefficient
    IF (ImagComp > 0) AND (RealComp > 0) THEN Phase(k) = ATN(ImagComp/RealComp)
    IF (ImagComp > 0) AND (RealComp < 0) THEN Phase(k) = fft.pi - ATN(-ImagComp/RealComp)
    IF (ImagComp < 0) AND (RealComp > 0) THEN Phase(k) = -1 * ATN(-ImagComp/RealComp)
    IF (ImagComp < 0) AND (RealComp < 0) THEN Phase(k) = -fft.pi + ATN(ImagComp/RealComp)
    IF (ImagComp = 0) AND (RealComp >= 0) THEN Phase(k) = 0.00
    IF (ImagComp = 0) AND (RealComp < 0) THEN Phase(k) = fft.pi
    IF (RealComp = 0) AND (ImagComp > 0) THEN Phase(k) = fft.pi / 2
    IF (RealComp = 0) AND (ImagComp < 0) THEN Phase(k) = -fft.pi / 2
    Phase(k) = Phase(k) * 180.0 / fft.pi         'convert to degrees
  NEXT k
  MEMCPY(pp, @Phase(0), Sample_N * SIZEOF(SINGLE))

END SUB






SUB FFT2DCalc (lpxreal AS SINGLE PTR, lpyImag AS SINGLE PTR, BYVAL c AS INTEGER, BYVAL r AS INTEGER, BYVAL flag AS INTEGER) EXPORT
  DIM rp As Single Ptr:  rp = @lpxReal :  DIM xReal(r, c) AS SINGLE : MEMCPY(@xReal(0, 0), rp, r * c * SIZEOF(SINGLE))
  DIM ip As Single Ptr:  ip = @lpyImag :  DIM yImag(r, c) AS SINGLE : MEMCPY(@yImag(0, 0), ip, r * c * SIZEOF(SINGLE))

     IF CheckPowerOfTwo(r) AND CheckPowerOfTwo(c) THEN     'make sure Radix-2
        IF (flag = 0) THEN
          FFT2DRCCalc(xReal(), yImag(), c, r, flag)    'real to complex
        ELSE
          FFT2DCRCalc(xReal(), yImag(), c, r, flag)    'complex to real
        END IF
        MEMCPY(rp, @xReal(0, 0),  r * c * SIZEOF(SINGLE))
        MEMCPY(ip, @yImag(0, 0),  r * c * SIZEOF(SINGLE))
     END IF
END SUB




SUB FFT2DCRCalc (xreal() AS SINGLE, yimag() AS SINGLE, c AS INTEGER, r AS INTEGER, flag AS INTEGER) 
    DIM i AS INTEGER, a AS INTEGER, j AS INTEGER
    DIM xVector(c - 1) AS SINGLE
    DIM yVector(c - 1) AS SINGLE

      FOR j = 0 TO r - 1
	   FOR a = 0 TO c - 1
		xVector(a) = xreal(j, a)
		yVector(a) = yimag(j, a)
	   NEXT
	   IF (flag = 0) THEN      'forward FFT
		fft(@xVector(0), @yVector(0), c, 0)
	   ELSE                    'inverse FFT
		fft(@xVector(0), @yVector(0), c, 1)
	   END IF
	   FOR a = 0 TO c - 1
	       xreal(j, a) = xVector(a)
	       yimag(j, a) = yVector(a)
	   NEXT
      NEXT

      FOR i = 0 TO c - 1
	   FOR a = 0 TO r - 1
		xVector(a) = xreal(a, i)
		yVector(a) = yimag(a, i)
	   NEXT
	   IF (flag = 0) THEN      'forward fft
		fft(@xVector(0), @yVector(0), r, 0)
	   ELSE                    'inverse fft
		fft(@xVector(0), @yVector(0), r, 1)
	   END IF
	   FOR a = 0 TO r - 1
		xreal(a, i) = xVector(a)
		yimag(a, i) = yVector(a)
	   NEXT
      NEXT
END SUB




'2d fast fourier transform from real to complex
' xreal() is a real 2D array of size(r,c) Radix-2
' yreal() is an imaginary 2D array of size(r,c) Radix-2
SUB FFT2DRCCalc (xreal() AS SINGLE, yimag() AS SINGLE, c AS INTEGER, r AS INTEGER, flag AS INTEGER)
    DIM i AS INTEGER, a AS INTEGER, j AS INTEGER
    DIM xVector(r - 1) AS SINGLE, yVector(r - 1) AS SINGLE


      FOR j = 0 TO c - 1
	   FOR a = 0 TO r - 1
		xVector(a) = xreal(a, j)
		yVector(a) = yimag(a, j)
	   NEXT a
	   IF (flag = 0) THEN      'forward fft
		fft(@xVector(0), @yVector(0), c, 0)
	   ELSE                    'inverse
		fft(@xVector(0), @yVector(0), c, 1)
	   END IF
	   FOR a = 0 TO r - 1
	       xreal(a, j) = xVector(a)
	       yimag(a, j) = yVector(a)
	   NEXT a
      NEXT j

      FOR i = 0 TO r - 1
	   FOR a = 0 TO c - 1
		xVector(a) = xreal(i, a)
		yVector(a) = yimag(i, a)
	   NEXT a

	   IF (flag = 0) THEN          'forward fft
		fft(@xVector(0), @yVector(0), r, 0)
	   ELSE                        'inverse fft
		fft(@xVector(0), @yVector(0), r, 1)
	   END IF
	   FOR a = 0 TO c - 1
		xreal(i, a) = xVector(a)
		yimag(i, a) = yVector(a)
	   NEXT a
      NEXT i

END SUB






SUB FFTFrequency (lpHz AS SINGLE PTR, BYVAL Sample_Hz AS SINGLE, BYVAL n AS INTEGER) EXPORT
    DIM k AS INTEGER
    DIM hzp As Single Ptr:  hzp = @lpHz :  DIM Hz(n) AS SINGLE : MEMCPY(@Hz(0), hzp, n * SIZEOF(SINGLE))

    FOR k = 0 TO (n/2)
      Hz(k) = k * Sample_Hz / n
    NEXT k

	FOR k = (n/2 + 1) TO n
      Hz(k) = -Sample_Hz * (n-k) / n
	NEXT k

    MEMCPY(hzp, @Hz(0), n * SIZEOF(SINGLE))
END SUB







' CalcFrequency() transforms a single frequency.
Sub CalcFrequency (BYVAL NumberOfSamples As Long, BYVAL FrequencyIndex As Long, lpRealIn AS SINGLE PTR, lpImagIn AS SINGLE PTR, BYREF RealOut AS SINGLE, BYREF ImagOut AS SINGLE) EXPORT
    Dim K As Long
    Dim Cos1 AS SINGLE, Cos2 AS SINGLE, Cos3 AS SINGLE, Theta AS SINGLE, Beta AS SINGLE
    Dim Sin1 AS SINGLE, Sin2 AS SINGLE, Sin3 AS SINGLE

    DIM rp As Single Ptr:  rp = @lpRealIn : DIM RealIn(NumberOfSamples) AS SINGLE : MEMCPY(@RealIn(0), rp, NumberOfSamples * SIZEOF(SINGLE))
    DIM ip As Single Ptr:  ip = @lpImagIn : DIM ImagIn(NumberOfSamples) AS SINGLE : MEMCPY(@ImagIn(0), ip, NumberOfSamples * SIZEOF(SINGLE))
    
    Theta = 2 * fft.pi * FrequencyIndex / NumberOfSamples
    Sin1 = Sin(-2 * Theta)
    Sin2 = Sin(-Theta)
    Cos1 = Cos(-2 * Theta)
    Cos2 = Cos(-Theta)
    Beta = 2 * Cos2
    
    For K = 0 To NumberOfSamples - 2
        'Update trig values
        Sin3 = Beta * Sin2 - Sin1
        Sin1 = Sin2
        Sin2 = Sin3
        
        Cos3 = Beta * Cos2 - Cos1
        Cos1 = Cos2
        Cos2 = Cos3
        
        RealOut = RealOut + RealIn(K) * Cos3 - ImagIn(K) * Sin3
        ImagOut = ImagOut + ImagIn(K) * Cos3 + RealIn(K) * Sin3
    Next k
    MEMCPY(rp, @RealIn(0), NumberOfSamples * SIZEOF(SINGLE))
    MEMCPY(ip, @ImagIn(0), NumberOfSamples * SIZEOF(SINGLE))

End Sub






SUB FFTMagnitude (lpxr AS SINGLE PTR, lpyi AS SINGLE PTR, BYVAL n AS INTEGER, lpOutVal AS SINGLE PTR) EXPORT
    DIM i  AS INTEGER
	DIM rr AS SINGLE, ii AS SINGLE
    DIM xp As Single Ptr:  xp = @lpxr : DIM xr(n) AS SINGLE : MEMCPY(@xr(0), xp, n * SIZEOF(SINGLE))
    DIM yp As Single Ptr:  yp = @lpyi : DIM yi(n) AS SINGLE : MEMCPY(@yi(0), yp, n * SIZEOF(SINGLE))
    DIM op As Single Ptr:  op = @lpOutVal : DIM OutVal(n) AS SINGLE : MEMCPY(@OutVal(0), op, n * SIZEOF(SINGLE))

      OutVal(0) = SQR(xr(0) * xr(0) + yi(0) * yi(0)) / n
      FOR i = 1 TO n
        rr = ABS(xr(i)) + ABS(xr(n - i))
    	ii = ABS(yi(i)) + ABS(yi(n - i))
        OutVal(i) = SQR(rr * rr + ii * ii) / n
      NEXT i
	MEMCPY(op, @OutVal(0), n * SIZEOF(SINGLE))
END SUB




SUB FFTPhase (lpxr AS SINGLE PTR, lpyi AS SINGLE PTR, BYVAL n AS INTEGER, lpPhase AS SINGLE PTR) EXPORT
   DIM xp As Single Ptr:  xp = @lpxr : DIM xr(n) AS SINGLE : MEMCPY(@xr(0), xp, n * SIZEOF(SINGLE))
   DIM yp As Single Ptr:  yp = @lpyi : DIM yi(n) AS SINGLE : MEMCPY(@yi(0), yp, n * SIZEOF(SINGLE))
   DIM pp As Single Ptr:  pp = @lpPhase : DIM Phase(n) AS SINGLE : MEMCPY(@Phase(0), pp, n * SIZEOF(SINGLE))

   DIM NearZero AS SINGLE: NearZero = 1E-20
   DIM TempAngle AS SINGLE
   DIM i AS INTEGER

   FOR i = 0 TO n
   IF ABS(xr(i)) < NearZero THEN
      IF ABS(yi(i)) < NearZero THEN TempAngle = 0!
      IF yi(i) < -(NearZero) THEN TempAngle = 3! * fft.pi / 2!
      IF yi(i) > NearZero THEN TempAngle = fft.pi / 2!
      Phase(i) = TempAngle
'      EXIT FOR
   ELSE
     IF ABS(yi(i)) < NearZero THEN
         IF (xr(i) < -(NearZero)) THEN TempAngle = fft.pi
	     IF xr(i) > NearZero THEN TempAngle = 0
	     Phase(i) = TempAngle
'	     EXIT FOR
     ELSE
        IF (xr(i) > NearZero)  AND (yi(i) > NearZero) THEN TempAngle = ATN(yi(i) / xr(i))
        IF (xr(i) < -NearZero) AND (yi(i) > NearZero) THEN TempAngle = fft.pi - ATN(-yi(i) / xr(i))
        IF (xr(i) < -NearZero) AND (yi(i) < -NearZero) THEN TempAngle = fft.pi + ATN(yi(i) / xr(i))
        IF (xr(i) > NearZero)  AND (yi(i) < -NearZero) THEN TempAngle = 2! * fft.pi - ATN(-yi(i) / xr(i))
     END IF
   END IF
   Phase(i) = TempAngle
   NEXT i

   MEMCPY(pp, @Phase(0), n * SIZEOF(SINGLE))
END SUB







'***************  power spectrum  **************
'    input real data (xreal)  ==> output power
'    input imag data (yimag) ==> output each Frequency, or Hz

SUB PowerSpectrumCalc (lpxreal AS SINGLE PTR, lpyimag AS SINGLE PTR, BYVAL numdat AS INTEGER, BYVAL delta AS SINGLE) EXPORT
    DIM numdatr AS SINGLE, normal AS SINGLE, timespan AS SINGLE
    DIM i AS INTEGER
    DIM rp As Single Ptr:  rp = @lpxReal :  DIM xReal(numdat) AS SINGLE : MEMCPY(@xReal(0), rp, numdat * SIZEOF(SINGLE))
    DIM ip As Single Ptr:  ip = @lpyImag :  DIM yImag(numdat) AS SINGLE : MEMCPY(@yImag(0), ip, numdat * SIZEOF(SINGLE))


    IF CheckPowerOfTwo(numdat) = 0 THEN EXIT SUB
    numdatr = numdat * 1!
    normal = 1! / numdatr ^ 2
    fft(@xreal(0), @yimag(0), numdat, 0)     'perform the fft 
    'now set up the arrays for power and frequency output
    xreal(0) = SquareAndSum(xreal(0), yimag(0)) * normal
    FOR i = 1 TO (numdat \ 2) - 1
      xreal(i) = normal * (SquareAndSum(xreal(i), yimag(i)) + SquareAndSum(xreal(numdat - i), yimag(numdat - i)))
    NEXT
    i = numdat \ 2
    xreal(i) = SquareAndSum(xreal(i), yimag(i)) * normal
    timespan = numdat * delta
    FOR i = 0 TO (numdat \ 2)
      yimag(i) = INT(i) / timespan
    NEXT
    MEMCPY(rp, @xReal(0), numdat * SIZEOF(SINGLE))
    MEMCPY(ip, @yImag(0), numdat * SIZEOF(SINGLE))
END SUB




SUB RealFFT2 (xr() AS SINGLE, sinc() AS SINGLE, n AS INTEGER, inverse AS INTEGER)
    DIM nn AS INTEGER, j AS INTEGER, m AS INTEGER
    DIM pp

    IF CheckPowerOfTwo(n) = 0 THEN EXIT SUB
      m = -1
      nn = n \ 2

      ' determine power of 2
      WHILE (nn > 0)
    	nn = nn \ 2
    	m = m + 1
      WEND
      nn = n \ 2

 IF (inverse = 1) THEN
	pp = .5 / (nn)
	trans(xr(), sinc(), nn, 1)
	fft2(xr(), sinc(), nn, m, nn)
	FOR j = 0 TO nn - 1
	  xr(j) = xr(j) * pp
	  sinc(j) = sinc(j) * (-pp)
	NEXT
	reorder(xr(), sinc(), nn, m, nn, 1)
	FOR j = 0 TO nn - 1     ' combine
	  xr(j + nn) = sinc(j)
	NEXT
      ELSE
	FOR j = 0 TO nn - 1           ' split array in 2
	  sinc(j) = xr(j + nn)
	NEXT
	reorder(xr(), sinc(), nn, m, nn, 1)
	rfft2(xr(), sinc(), nn, m, 1)
	FOR j = 0 TO nn - 1
	  xr(j) = xr(j) * .5
	  sinc(j) = sinc(j) * .5
	NEXT
	trans(xr(), sinc(), nn, 0)
	FOR j = 0 TO nn - 1
	  sinc(j) = -sinc(j)
	NEXT
	FOR j = 1 TO nn - 1
	  xr(j + nn) = xr(nn - j)
	  sinc(j + nn) = -sinc(nn - j)
	NEXT
  END IF
END SUB


SUB reorder (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER, reel AS INTEGER)
      DIM i AS INTEGER, j AS INTEGER, indx AS INTEGER, k AS INTEGER, kk AS INTEGER, kb AS INTEGER, k2 AS INTEGER, KU AS INTEGER, lim AS INTEGER, p AS INTEGER
      DIM breakf AS INTEGER
      DIM temp
      DIM cc(m) AS INTEGER
      DIM lst(m)

      cc(m) = ks
      FOR k = m TO 1 STEP -1
	cc(k - 1) = cc(k) \ 2
      NEXT
      p = m - 1
      j = m - 1
      i = 0
      kb = 0
      IF (reel <> 0) THEN
	KU = n - 2
	FOR k = 0 TO KU STEP 2
	  temp = xr(k + 1)
	  xr(k + 1) = y(k)
	  y(k) = temp
	NEXT
      ELSE
	  m = m - 1
      END IF
      lim = (m + 2) \ 2
      breakf = 0
      IF (p > 0) THEN
	WHILE (breakf = 0)

3000      KU = cc(j) + kb
	  k2 = KU
	  indx = cc(m - j)
	  kk = kb + indx

	  WHILE (kk < KU)
	     k = kk + indx
	     WHILE (kk < k)
	       temp = xr(kk)
	       xr(kk) = xr(k2)
	       xr(k2) = temp
	       temp = y(kk)
	       y(kk) = y(k2)
	       y(k2) = temp
	       kk = kk + 1
	       k2 = k2 + 1
	     WEND 'kk < k

	     kk = kk + indx
	     k2 = k2 + indx
	  WEND ' KK < KU

	  IF (j > lim) THEN
	    j = j - 1
	    i = i + 1
	    lst(i) = j
	    GOTO 3000
	  END IF
	  kb = k2
	  IF (i > 0) THEN
	    j = lst(i)
	    i = i - 1
	    GOTO 3000
	  END IF
	  IF (kb < n) THEN
	    j = p
	  ELSE
	   breakf = 1
	   GOTO 3010
	  END IF
3010    ' CONTINUE
	WEND ' True

      END IF
END SUB



SUB rfft2 (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, m AS INTEGER, ks AS INTEGER)
    DIM k0 AS INTEGER, k1 AS INTEGER, k2 AS INTEGER, k3 AS INTEGER, k4 AS INTEGER, span AS INTEGER, j AS INTEGER, indx AS INTEGER, k AS INTEGER, kb AS INTEGER, nt AS INTEGER, kn AS INTEGER, mk AS INTEGER
    DIM Breakf1 AS INTEGER, fp AS INTEGER
    DIM rad AS SINGLE, c1 AS SINGLE, c2 AS SINGLE, c3 AS SINGLE, s1 AS SINGLE
    DIM s2 AS SINGLE, s3 AS SINGLE, ck AS SINGLE, sk AS SINGLE, sq AS SINGLE
    DIM x0 AS SINGLE, x1 AS SINGLE, x2 AS SINGLE, x3 AS SINGLE
    DIM y0 AS SINGLE, y1 AS SINGLE, y2 AS SINGLE, y3 AS SINGLE, re AS SINGLE, im AS SINGLE
    DIM cc(m) AS INTEGER

      sq = .707106781187#
      sk = .382683432366#
      ck = .92387953251#

      cc(0) = ks
      kn = 0
      k4 = 4 * ks
      mk = m - 4

      FOR k = 1 TO m
	ks = ks + ks
	cc(k) = ks
      NEXT
      rad = 3.14159265359# / ((cc(0)) * (ks))

      WHILE (kn < n)
	kb = kn + 4
	kn = kn + ks
	IF (m <> 1) THEN
	  k = 0
	  indx = 0
	  j = mk
	  nt = 3
	  c1 = 1!
	  s1 = 0!
	  Breakf1 = 0
	  WHILE (Breakf1 = 0)
2000        span = cc(k)
	    fp = 0
	    IF (indx <> 0) THEN
	       c2 = (indx) * (span) * rad
	       c1 = COS(c2)
	       s1 = SIN(c2)
	    END IF
2001        IF ((fp = 1) OR (indx <> 0)) THEN
	      c2 = c1 * c1 - s1 * s1
	      s2 = 2! * s1 * c1
	      c3 = c2 * c1 - s2 * s1
	      s3 = c2 * s1 + s2 * c1
	    ELSE
	      s1 = 0!
	    END IF
	    k3 = kb - span
	    WHILE (k3 < kb)
	      k2 = k3 - span
	      k1 = k2 - span
	      k0 = k1 - span

	      x0 = xr(k0)
	      y0 = y(k0)
	      x1 = xr(k1)
	      y1 = y(k1)
	      x2 = xr(k2)
	      y2 = y(k2)
	      x3 = xr(k3)
	      y3 = y(k3)

	      xr(k0) = x0 + x2 + x1 + x3
	      y(k0) = y0 + y2 + y1 + y3
	      IF (s1 = 0!) THEN
		xr(k1) = x0 - x1 - y2 + y3
		y(k1) = y0 - y1 + x2 - x3
		xr(k2) = x0 + x1 - x2 - x3
		y(k2) = y0 + y1 - y2 - y3
		xr(k3) = x0 - x1 + y2 - y3
		y(k3) = y0 - y1 - x2 + x3
	      ELSE
		re = x0 - x1 - y2 + y3
		im = y0 - y1 + x2 - x3
		xr(k1) = re * c1 - im * s1
		y(k1) = re * s1 + im * c1
		re = x0 + x1 - x2 - x3
		im = y0 + y1 - y2 - y3
		xr(k2) = re * c2 - im * s2
		y(k2) = re * s2 + im * c2
		re = x0 - x1 + y2 - y3
		im = y0 - y1 - x2 + x3
		xr(k3) = re * c3 - im * s3
		y(k3) = re * s3 + im * c3
	      END IF 's1 = 0
	      k3 = k3 + 1
	    WEND  'while (k3 < kb)
	    nt = nt - 1
	    IF (nt >= 0) THEN
	      c2 = c1
	      IF (nt = 1) THEN
		c1 = c1 * ck + s1 * sk
		s1 = s1 * ck - c2 * sk
	      ELSE
		c1 = (c1 - s1) * sq
		s1 = (s1 + c2) * sq
	      END IF
	      kb = kb + k4
	      IF (kb <= kn) THEN
		fp = 1
		GOTO 2001
	      ELSE
		Breakf1 = 1
		GOTO 2005
	      END IF
	    END IF  'nt >= 0
	    IF (nt = -1) THEN
	      k = 2
	      GOTO 2000
	    END IF
	    IF (cc(j) <= indx) THEN
	      indx = indx - cc(j)
	      j = j - 1
	      IF (cc(j) <= indx) THEN
		indx = indx - cc(j)
		j = j - 1
		k = k + 2
	      ELSE
		indx = cc(j) + indx
		j = mk
	      END IF
	    ELSE
	      indx = cc(j) + indx
	      j = mk
	    END IF 'cc[j] <= indx
	    IF (j < mk) THEN GOTO 2000
	    k = 0
	    nt = 3
	    kb = kb + k4
	    IF (kb > kn) THEN Breakf1 = 1
2005        ' CONTINUE
	  WEND  ' while true
	END IF  'm <> 1
	k = (m \ 2) * 2

	IF (k <> m) THEN
	  k2 = kn
	  k0 = kn - cc(k)
	  j = k0
	  WHILE (k2 > j)
	    k2 = k2 - 1
	    k0 = k0 - 1
	    x0 = xr(k2)
	    y0 = y(k2)
	    xr(k2) = xr(k0) - x0
	    xr(k0) = xr(k0) + x0
	    y(k2) = y(k0) - y0
	    y(k0) = y(k0) + y0
	  WEND  'while (k2 > j)
	END IF    ' k <> m
      WEND         'while (kn < n)
END SUB



FUNCTION SquareAndSum (BYVAL a AS SINGLE, BYVAL b AS SINGLE) AS SINGLE EXPORT
  SquareAndSum = a ^ 2 + b ^ 2
END FUNCTION



SUB trans (xr() AS SINGLE, y() AS SINGLE, n AS INTEGER, eval AS INTEGER)
    DIM k AS INTEGER, nk AS INTEGER, nn AS INTEGER
    DIM x1 AS SINGLE, ab AS SINGLE, ba AS SINGLE, y1 AS SINGLE
    DIM re AS SINGLE, im AS SINGLE, ck AS SINGLE, sk AS SINGLE, dc AS SINGLE, ds AS SINGLE, r AS SINGLE

      nn = n \ 2
      r = 3.14159265359# / (n)
      ds = SIN(r)
      r = 2! * SIN(.5 * r)
      r = -r * r
      dc = -.5 * r
      ck = 1!
      sk = 0!

      IF (eval = 0) THEN
	xr(n) = xr(0)
	y(n) = y(0)
      END IF
      FOR k = 0 TO nn
	nk = n - k
	x1 = xr(k) + xr(nk)
	ab = xr(k) - xr(nk)
	ba = y(k) + y(nk)
	y1 = y(k) - y(nk)

	re = ck * ba + sk * ab
	im = sk * ba - ck * ab
	y(nk) = im - y1
	y(k) = im + y1
	xr(nk) = x1 - re
	xr(k) = x1 + re
	dc = r * ck + dc
	ck = ck + dc
	ds = r * sk + ds
	sk = sk + ds
      NEXT
END SUB



SUB WindowData (lpx AS SINGLE PTR, numdat AS INTEGER, wind AS INTEGER) EXPORT
    DIM multiplier AS SINGLE, i AS INTEGER
    DIM xp As Single Ptr:  xp = @lpx :  DIM x(numdat)AS SINGLE : MEMCPY(@x(0), xp, numdat * SIZEOF(SINGLE))


   FOR i = 0 TO numdat - 1
    SELECT CASE wind
       CASE 0
	     multiplier = 1
       CASE 1
	     multiplier = Parzen(numdat, i)
       CASE 2
	     multiplier = Hanning(numdat, i)
       CASE 3
	      multiplier = Welch(numdat, i)
       CASE 4
	      multiplier = Hamming(numdat, i)
       CASE 5
	      multiplier = ExactBlackman(numdat, i)
       CASE ELSE
	      multiplier = 1!
    END SELECT
    x(i) = multiplier * x(i)
  NEXT i
    MEMCPY(xp, @x(0), numdat * SIZEOF(SINGLE))
END SUB


' ****************  window filters  ***********************

FUNCTION ExactBlackman (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
  ExactBlackman = .42 - .5 * COS(2! * fft.pi * j / (n - 1)) + .08 * COS(4! * fft.pi * j / (n - 1))
END FUNCTION

FUNCTION Hamming (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
  Hamming = .54 - .46 * COS(2! * fft.pi * j / (n - 1))
END FUNCTION

FUNCTION Hanning (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
  Hanning = .5 * (1 - COS(2! * fft.pi * j / (n - 1!)))
END FUNCTION

FUNCTION Parzen (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
  Parzen = 1 - ABS((j - .5 * (n - 1)) / (.5 * (n - 1)))
END FUNCTION

FUNCTION SigmaGauss (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
    SigmaGauss = Exp(-4.5 * (2 * j / n - 1) ^ 2) 
END FUNCTION


FUNCTION Welch (n AS INTEGER, j AS INTEGER) AS SINGLE EXPORT
  Welch = 1 - ((j - .5 * (n - 1)) / (.5 * (n + 1))) ^ 2
END FUNCTION




'--------------------------------------------------------------------
' VB FFT Release 2-B
' by Murphy McCauley (MurphyMc@Concentric.NET)
' 10/01/99
'--------------------------------------------------------------------
' About:
' This code is very, very heavily based on Don Cross's fourier.pas
' Turbo Pascal Unit for calculating the Fast Fourier Transform.
' I've not implemented all of his functions, though I may well do
' so in the future.
' For more info, you can contact me by email, check my website at:
' http://www.fullspectrum.com/deeth/
' or check Don Cross's FFT web page at:
' http://www.intersrv.com/~dcross/fft.html
' You also may be intrested in the FFT.DLL that I put together based
' on Don Cross's FFT C code.  It's callable with Visual Basic and
' includes VB declares.  You can get it from either website.
'--------------------------------------------------------------------
' History of Release 2-B:
' Fixed a couple of errors that resulted from me mucking about with
'   variable names after implementation and not re-checking.  BAD ME.
'  --------
' History of Release 2:
' Added FrequencyOfIndex() which is Don Cross's Index_to_frequency().
' FourierTransform() can now do inverse transforms.
' Added CalcFrequency() which can do a transform for a single
'   frequency.
'--------------------------------------------------------------------
' Usage:
' The useful functions are:
' FourierTransform() performs a Fast Fourier Transform on an pair of
'  Double arrays -- one real, one imaginary.  Don't want/need
'  imaginary numbers?  Just use an array of 0s.  This function can
'  also do inverse FFTs.
' FrequencyOfIndex() can tell you what actual frequency a given index
'  corresponds to.
' CalcFrequency() transforms a single frequency.
'--------------------------------------------------------------------
' Notes:
' All arrays must be 0 based (i.e. Dim TheArray(0 To 1023) or
'  Dim TheArray(1023)).
' The number of samples must be a power of two (i.e. 2^x).
' FrequencyOfIndex() and CalcFrequency() haven't been tested much.
' Use this ENTIRELY AT YOUR OWN RISK.
'--------------------------------------------------------------------



Function NumberOfBitsNeeded(PowerOfTwo As Long) As Byte
    Dim I As Byte
    For I = 0 To 16
        If (PowerOfTwo And (2 ^ I)) <> 0 Then
            NumberOfBitsNeeded = I
            Exit Function
        End If
    Next
End Function



Function ReverseBits(ByVal Index As Long, NumBits As Byte) As Long
    Dim I As Byte, Rev As Long
    
    For I = 0 To NumBits - 1
        Rev = (Rev * 2) Or (Index And 1)
        Index = Index \ 2
    Next
    ReverseBits = Rev
End Function




'******************************************************************
' Fourier transform :
'   RealIn() real part in --> real part out
'   yi() imaginary part in --> imaginary part out
'   n sample size
'   inverse -- if true then perform inverse Fourier transform
'******************************************************************
SUB fft (lpRealIn AS SINGLE PTR, lpImagIn AS SINGLE PTR, BYVAL NumSamples AS INTEGER, BYVAL inverse AS INTEGER) EXPORT

	IF CheckPowerOfTwo(NumSamples) = 0 THEN EXIT SUB         'can't do Radix-N

    'must transfer the data at the pointer to an internal array
'    DIM rp As Single Ptr:  rp = @lpRealIn : DIM RealIn(NumSamples) AS SINGLE : MEMCPY(@RealIn(0), rp, NumSamples * SIZEOF(SINGLE))
	DIM RealIn As Single PTR: RealIn = @lpRealIn

    DIM ip As Single Ptr:  ip = @lpImagIn : DIM ImagIn(NumSamples) AS SINGLE : MEMCPY(@ImagIn(0), ip, NumSamples * SIZEOF(SINGLE))
	DIM RealOut(NumSamples) as single		'these hold temp output of real / imag
	DIM ImagOut(NumSamples) as single

    Dim AngleNumerator as single
    Dim NumBits As Byte, I As Long, j As Long, K As Long, n As Long, BlockSize As Long, BlockEnd As Long
    Dim DeltaAngle as single, DeltaAr as single
    Dim Alpha as single, Beta as single
    Dim TR as single, TI as single, AR as single, AI as single
    
    IF Inverse > 0 THEN
		AngleNumerator = -2# * fft.pi
	ELSE
	    AngleNumerator = 2# * fft.pi
	END IF
   
    NumBits = NumberOfBitsNeeded(NumSamples)
    For I = 0 To (NumSamples - 1)
        j = ReverseBits(I, NumBits)
        RealOut(j) = RealIn[I] 'RealIn(I) '
        ImagOut(j) = ImagIn(I)
    Next
    

    BlockEnd = 1
    BlockSize = 2
    
    Do While BlockSize <= NumSamples
        DeltaAngle = AngleNumerator / BlockSize
        Alpha = Sin(0.5 * DeltaAngle)
        Alpha = 2# * Alpha * Alpha
        Beta = Sin(DeltaAngle)
        
        I = 0
        Do While I < NumSamples
            AR = 1#
            AI = 0#
            
            j = I
            For n = 0 To BlockEnd - 1
                K = j + BlockEnd
                TR = AR * RealOut(K) - AI * ImagOut(K)
                TI = AI * RealOut(K) + AR * ImagOut(K)
                RealOut(K) = RealOut(j) - TR
                ImagOut(K) = ImagOut(j) - TI
                RealOut(j) += TR
                ImagOut(j) += TI

                DeltaAr = Alpha * AR + Beta * AI
                AI -= (Alpha * AI - Beta * AR)
                AR -= DeltaAr
                j += 1
            Next n
            
            I += BlockSize
        Loop
        
        BlockEnd = BlockSize
        BlockSize = BlockSize * 2
    Loop

    If Inverse Then

        'Normalize the resulting time samples...
        For I = 0 To NumSamples - 1
            RealOut(I) /= NumSamples
            ImagOut(I) /= NumSamples
        Next I
    End If

'    MEMCPY(rp, @RealOut(0), NumSamples * SIZEOF(SINGLE))      'restore the arrays to calling program
    MEMCPY(RealIn, @RealOut(0), NumSamples * SIZEOF(SINGLE))      'restore the arrays to calling program
    MEMCPY(ip, @ImagOut(0), NumSamples * SIZEOF(SINGLE))
End Sub




