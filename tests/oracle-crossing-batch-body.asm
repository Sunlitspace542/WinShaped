	ifne	DO_HDR

DOS
	ShapeHdr	DOS_P,0,DOS_F,0,0,0,0,0,14,10,10,10,14,,0,0,0,0,<DOS>
	elseif
DOS_P
	Pointsb	4
	pb	0,10,-10	;0
	pb	0,-10,-10	;1
	pb	0,10,10	;2
	pb	0,-10,10	;3
	PointsXb	2
	pb	10,5,0	;4
	pb	-10,-5,0	;6

	EndPoints
DOS_F
	Vizis	2
	Viz	4,5,6,0,0,127	;0
	Viz	0,1,3,127,0,0	;1

DOS_f1	Faces
	Face4	2,0,0,0,127,4,5,6,7
	Face4	1,1,127,0,0,0,1,3,2
	Fend
	EndShape

	endc
