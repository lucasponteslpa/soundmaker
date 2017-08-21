section .data
	c dq 5.5

section .text
global seno
global pulso

seno:
	enter 0,0
	finit
	fld  dword [ebp+8]
	mov ecx, dword[ebp+12]
	fsin
	fst dword [ecx]
	leave
	ret

pulso:
	enter 0,0
	finit
	fld  dword [ebp+8]
	fld qword [c]
	fmulp st1, st0
	fsin
	fld  dword [ebp+8]
	mov ecx, dword[ebp+12]
	fsin
	fmulp st1, st0
	fst dword [ecx]
	leave
	ret
