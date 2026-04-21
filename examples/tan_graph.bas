10 SCREEN 2
20 CLS
30 COLOR 15, 0
40 REM tan(x^2 + y^2) = 1
50 REM This produces concentric rings
60 CX = 320
70 CY = 100
80 SX = 0.03
90 SY = 0.03
100 EPS = 0.08
110 FOR PY = 0 TO 199
120   FOR PX = 0 TO 639
130     X = (PX - CX) * SX
140     Y = (PY - CY) * SY
150     R2 = X * X + Y * Y
160     V = TAN(R2)
170     IF ABS(V - 1) < EPS THEN PSET (PX, PY), 10
180   NEXT PX
190 NEXT PY
200 LOCATE 1, 1
210 PRINT "tan(x^2+y^2)=1"
220 END