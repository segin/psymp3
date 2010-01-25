	.intel_syntax noprefix

	#kwikpad.bas' compilation started at 16:22:27 (FreeBASIC v0.21.0b)

.section .text
.balign 16

.globl _GETFILENAME@4
_GETFILENAME@4:
push ebp
mov ebp, esp
sub esp, 9072
push ebx
mov dword ptr [ebp-12], 0
mov dword ptr [ebp-8], 0
mov dword ptr [ebp-4], 0
.Lt_002F:
push 120
push 320
push 120
push 160
lea eax, [ebp-7932]
push eax
call __ZN5_KGUIC1Eiiii
add esp, 20
push offset _MESSAGEHANDLER@8
push 3
lea eax, [ebp-8212]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 4
lea eax, [ebp-8492]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 4
lea eax, [ebp-8772]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 1
lea eax, [ebp-9052]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
lea eax, [ebp-8212]
mov dword ptr [ebp-9056], eax
push 0
push 10
push offset _Lt_0037
push -1
mov eax, dword ptr [ebp-9056]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-9056]
mov dword ptr [eax+32], 7368720
mov eax, dword ptr [ebp-9056]
mov dword ptr [eax+44], 0
lea eax, [ebp-8212]
push eax
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-8492]
mov dword ptr [ebp-9060], eax
mov eax, dword ptr [ebp-9060]
mov dword ptr [eax+12], 220
mov eax, dword ptr [ebp-9060]
mov dword ptr [eax+16], 40
mov eax, dword ptr [ebp-9060]
mov dword ptr [eax+20], 75
mov eax, dword ptr [ebp-9060]
mov dword ptr [eax+24], 25
mov eax, dword ptr [ebp-9060]
mov dword ptr [eax+64], 2
push 0
push 3
push offset _Lt_0039
push -1
mov eax, dword ptr [ebp-9060]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 3
push offset _Lt_0039
push -1
mov eax, dword ptr [ebp-9060]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-8492]
push eax
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-8772]
mov dword ptr [ebp-9064], eax
mov eax, dword ptr [ebp-9064]
mov dword ptr [eax+12], 220
mov eax, dword ptr [ebp-9064]
mov dword ptr [eax+16], 75
mov eax, dword ptr [ebp-9064]
mov dword ptr [eax+20], 75
mov eax, dword ptr [ebp-9064]
mov dword ptr [eax+24], 25
mov eax, dword ptr [ebp-9064]
mov dword ptr [eax+64], 2
push 0
push 7
push offset _Lt_003B
push -1
mov eax, dword ptr [ebp-9064]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 7
push offset _Lt_003C
push -1
mov eax, dword ptr [ebp-9064]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-8772]
push eax
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-9052]
mov dword ptr [ebp-9068], eax
mov eax, dword ptr [ebp-9068]
mov dword ptr [eax+12], 20
mov eax, dword ptr [ebp-9068]
mov dword ptr [eax+16], 60
mov eax, dword ptr [ebp-9068]
mov dword ptr [eax+20], 175
mov eax, dword ptr [ebp-9068]
mov dword ptr [eax+24], 20
push 0
push 12
push offset _Lt_003E
push -1
mov eax, dword ptr [ebp-9068]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-9052]
push eax
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
mov dword ptr [ebp-7920], 1
mov dword ptr [ebp-7928], 200
mov dword ptr [ebp-7900], 1
.Lt_003F:
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI13PROCESSEVENTSEv@4
call _fb_GfxLock@0
push 0
push dword ptr [ebp+8]
call __ZN5_KGUI6RENDEREPv@8
push 0
lea eax, [ebp-7932]
push eax
call __ZN5_KGUI6RENDEREPv@8
push -1
push -1
call _fb_GfxUnlock@8
mov eax, dword ptr [_SELECTION]
mov dword ptr [ebp-9072], eax
cmp dword ptr [ebp-9072], 3
jne .Lt_0044
.Lt_0045:
push 0
push -1
lea eax, [ebp-8876]
push eax
push -1
lea eax, [ebp-12]
push eax
call _fb_StrAssign@20
lea eax, [ebp-9052]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8772]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8492]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8212]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-7932]
push eax
call __ZN5_KGUID1Ev
add esp, 4
jmp .Lt_0030
jmp .Lt_0042
.Lt_0044:
cmp dword ptr [ebp-9072], 4
jne .Lt_0046
.Lt_0047:
jmp .Lt_0040
.Lt_0046:
.Lt_0042:
mov dword ptr [_SELECTION], 0
push 3
call _fb_Sleep@4
.Lt_0041:
jmp .Lt_003F
.Lt_0040:
lea eax, [ebp-9052]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8772]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8492]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8212]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-7932]
push eax
call __ZN5_KGUID1Ev
add esp, 4
.Lt_0030:
lea eax, [ebp-12]
push eax
call _fb_StrAllocTempResult@4
pop ebx
mov esp, ebp
pop ebp
ret 4
.balign 16

.globl _MESSAGEHANDLER@8
_MESSAGEHANDLER@8:
push ebp
mov ebp, esp
sub esp, 28
push ebx
mov dword ptr [ebp-4], 0
.Lt_0048:
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [eax+4]
mov dword ptr [ebp-8], ebx
mov ebx, dword ptr [ebp-8]
mov dword ptr [ebp-12], ebx
mov ebx, dword ptr [ebp+8]
mov dword ptr [ebp-16], ebx
cmp dword ptr [ebp-16], 0
jne .Lt_004D
.Lt_004E:
jmp .Lt_004B
.Lt_004D:
cmp dword ptr [ebp-16], 1
jne .Lt_004F
.Lt_0050:
push 0
push -1
mov ebx, dword ptr [ebp-12]
lea eax, [ebx+164]
push eax
push -1
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
mov dword ptr [ebp-20], 0
lea eax, [ebp-28]
push eax
call _fb_StrAssign@20
push 3
push offset _Lt_0039
push -1
lea eax, [ebp-28]
push eax
call _fb_StrCompare@16
test eax, eax
jne .Lt_0053
.Lt_0054:
mov dword ptr [_SELECTION], 3
jmp .Lt_0051
.Lt_0053:
push 7
push offset _Lt_003C
push -1
lea eax, [ebp-28]
push eax
call _fb_StrCompare@16
test eax, eax
jne .Lt_0055
.Lt_0056:
mov dword ptr [_SELECTION], 4
.Lt_0055:
.Lt_0051:
lea eax, [ebp-28]
push eax
call _fb_StrDelete@4
jmp .Lt_004B
.Lt_004F:
cmp dword ptr [ebp-16], 2
jne .Lt_0057
.Lt_0058:
jmp .Lt_004B
.Lt_0057:
cmp dword ptr [ebp-16], 3
jne .Lt_0059
.Lt_005A:
jmp .Lt_004B
.Lt_0059:
cmp dword ptr [ebp-16], 4
jne .Lt_005B
.Lt_005C:
jmp .Lt_004B
.Lt_005B:
cmp dword ptr [ebp-16], 5
jne .Lt_005D
.Lt_005E:
jmp .Lt_004B
.Lt_005D:
cmp dword ptr [ebp-16], 6
jne .Lt_005F
.Lt_0060:
jmp .Lt_004B
.Lt_005F:
cmp dword ptr [ebp-16], 8
jne .Lt_0061
.Lt_0062:
jmp .Lt_004B
.Lt_0061:
cmp dword ptr [ebp-16], 9
jne .Lt_0063
.Lt_0064:
push 0
push -1
mov eax, dword ptr [ebp-12]
lea ebx, [eax+128]
push ebx
push -1
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
mov dword ptr [ebp-20], 0
lea ebx, [ebp-28]
push ebx
call _fb_StrAssign@20
push 5
push offset _Lt_0068
push -1
lea eax, [ebp-28]
push eax
call _fb_StrCompare@16
test eax, eax
jne .Lt_0067
.Lt_0069:
mov dword ptr [_SELECTION], 1
jmp .Lt_0065
.Lt_0067:
push 5
push offset _Lt_006B
push -1
lea eax, [ebp-28]
push eax
call _fb_StrCompare@16
test eax, eax
jne .Lt_006A
.Lt_006C:
mov dword ptr [_SELECTION], 2
.Lt_006A:
.Lt_0065:
lea eax, [ebp-28]
push eax
call _fb_StrDelete@4
jmp .Lt_004B
.Lt_0063:
cmp dword ptr [ebp-16], 10
jne .Lt_006D
.Lt_006E:
.Lt_006D:
.Lt_004B:
mov dword ptr [ebp-4], 0
.Lt_0049:
mov eax, dword ptr [ebp-4]
pop ebx
mov esp, ebp
pop ebp
ret 8
.balign 16

.globl _main
_main:
push ebp
mov ebp, esp
and esp, 0xFFFFFFF0
sub esp, 8508
push ebx
mov dword ptr [ebp-4], 0
call ___main
push 0
push dword ptr [ebp+12]
push dword ptr [ebp+8]
call _fb_Init@12
.Lt_0001:
push 0
push 0
push 0
push 32
push 480
push 640
call _fb_GfxScreenRes@24
push 480
push 640
push 0
push 0
lea eax, [ebp-7924]
push eax
call __ZN5_KGUIC1Eiiii
add esp, 20
push 7
push offset _Lt_001D
call _fb_StrAllocTempDescZEx@8
push eax
call _fb_GfxSetWindowTitle@4
push offset _MESSAGEHANDLER@8
push 7
lea eax, [ebp-8204]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 16
lea eax, [ebp-8484]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
lea eax, [ebp-8204]
mov dword ptr [ebp-8488], eax
push 0
push 35
push offset _Lt_0021
push -1
mov eax, dword ptr [ebp-8488]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 9
push offset _Lt_0022
push -1
mov eax, dword ptr [ebp-8488]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-8488]
mov dword ptr [eax+32], 7368720
lea eax, [ebp-8204]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-8484]
mov dword ptr [ebp-8492], eax
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+12], 4
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+16], 30
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+20], 630
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+24], 440
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+32], 14737632
push 0
push 10
push offset _Lt_0024
push -1
mov eax, dword ptr [ebp-8492]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+200], 1
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+28], 0
push 0
push 1
push offset _Lt_0000
push -1
mov eax, dword ptr [ebp-8492]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+116], 1
mov eax, dword ptr [ebp-8492]
mov dword ptr [eax+192], 0
lea eax, [ebp-8484]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
.Lt_0025:
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI13PROCESSEVENTSEv@4
call _fb_GfxLock@0
push 0
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI6RENDEREPv@8
push -1
push -1
call _fb_GfxUnlock@8
mov eax, dword ptr [_SELECTION]
mov dword ptr [ebp-8496], eax
cmp dword ptr [ebp-8496], 1
jne .Lt_002A
.Lt_002B:
push 0
push -1
mov dword ptr [ebp-8508], 0
mov dword ptr [ebp-8504], 0
mov dword ptr [ebp-8500], 0
push 0
push -1
lea eax, [ebp-7924]
push eax
call _GETFILENAME@4
push eax
push -1
lea eax, [ebp-8508]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-8508]
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI12LOADTEXTFILEE8FBSTRING@8
lea ebx, [ebp-8508]
push ebx
mov ebx, eax
call _fb_StrDelete@4
push ebx
push -1
lea ebx, [ebp-8308]
push ebx
call _fb_StrAssign@20
jmp .Lt_0028
.Lt_002A:
cmp dword ptr [ebp-8496], 2
jne .Lt_002D
.Lt_002E:
jmp .Lt_0026
.Lt_002D:
.Lt_0028:
mov dword ptr [_SELECTION], 0
push 3
call _fb_Sleep@4
.Lt_0027:
push 1
call _fb_Multikey@4
test eax, eax
je .Lt_0025
.Lt_0026:
lea eax, [ebp-8484]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8204]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-7924]
push eax
call __ZN5_KGUID1Ev
add esp, 4
.Lt_0002:
push 0
call _fb_End@4
mov eax, dword ptr [ebp-4]
pop ebx
mov esp, ebp
pop ebp
ret
.balign 16
__ZN12_WORKER_VARSC1Ev:
push ebp
mov ebp, esp
.Lt_0004:
mov eax, dword ptr [ebp+8]
mov dword ptr [eax], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+4], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+8], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+12], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+16], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+20], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+24], 0
mov eax, dword ptr [ebp+8]
mov dword ptr [eax+32], 0
mov dword ptr [eax+36], 0
call _fb_Timer@0
mov eax, dword ptr [ebp+8]
fstp qword ptr [eax+40]
call _fb_Timer@0
mov eax, dword ptr [ebp+8]
fstp qword ptr [eax+48]
.Lt_0005:
mov esp, ebp
pop ebp
ret
.balign 16
__ZN12_WORKER_VARSaSERS_:
push ebp
mov ebp, esp
push ebx
.Lt_0006:
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax]
mov dword ptr [ebx], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+4]
mov dword ptr [ebx+4], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+8]
mov dword ptr [ebx+8], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+12]
mov dword ptr [ebx+12], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+16]
mov dword ptr [ebx+16], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+20]
mov dword ptr [ebx+20], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+24]
mov dword ptr [ebx+24], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
movlpd xmm7, qword ptr [ecx+32]
movlpd qword ptr [ebx+32], xmm7
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
movlpd xmm7, qword ptr [ecx+40]
movlpd qword ptr [ebx+40], xmm7
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
movlpd xmm7, qword ptr [ecx+48]
movlpd qword ptr [ebx+48], xmm7
.Lt_0007:
pop ebx
mov esp, ebp
pop ebp
ret
.balign 16
__ZN13_KGUI_CONTROLaSERS_:
push ebp
mov ebp, esp
push ebx
.Lt_000B:
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax]
mov dword ptr [ebx], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+4]
mov dword ptr [ebx+4], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+8]
mov dword ptr [ebx+8], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+12]
mov dword ptr [ebx+12], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+16]
mov dword ptr [ebx+16], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+20]
mov dword ptr [ebx+20], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+24]
mov dword ptr [ebx+24], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+28]
mov dword ptr [ebx+28], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+32]
mov dword ptr [ebx+32], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+36]
mov dword ptr [ebx+36], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+40]
mov dword ptr [ebx+40], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+44]
mov dword ptr [ebx+44], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+48]
mov dword ptr [ebx+48], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
movlpd xmm7, qword ptr [ecx+56]
movlpd qword ptr [ebx+56], xmm7
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+64]
mov dword ptr [ebx+64], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+68]
mov dword ptr [ebx+68], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+72]
mov dword ptr [ebx+72], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+76]
mov dword ptr [ebx+76], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+80]
mov dword ptr [ebx+80], eax
push 0
push -1
mov eax, dword ptr [ebp+12]
lea ebx, [eax+84]
push ebx
push -1
mov ebx, dword ptr [ebp+8]
lea eax, [ebx+84]
push eax
call _fb_StrAssign@20
push 0
push -1
mov eax, dword ptr [ebp+12]
lea ebx, [eax+96]
push ebx
push -1
mov ebx, dword ptr [ebp+8]
lea eax, [ebx+96]
push eax
call _fb_StrAssign@20
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+108]
mov dword ptr [ebx+108], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+112]
mov dword ptr [ebx+112], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+116]
mov dword ptr [ebx+116], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+120]
mov dword ptr [ebx+120], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+124]
mov dword ptr [ebx+124], ecx
push 0
push -1
mov ecx, dword ptr [ebp+12]
lea ebx, [ecx+128]
push ebx
push -1
mov ebx, dword ptr [ebp+8]
lea ecx, [ebx+128]
push ecx
call _fb_StrAssign@20
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+140]
mov dword ptr [ecx+140], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+144]
mov dword ptr [ecx+144], eax
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+148]
mov dword ptr [ecx+148], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+152]
mov dword ptr [ecx+152], eax
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+156]
mov dword ptr [ecx+156], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+160]
mov dword ptr [ecx+160], eax
push 0
push -1
mov eax, dword ptr [ebp+12]
lea ecx, [eax+164]
push ecx
push -1
mov ecx, dword ptr [ebp+8]
lea eax, [ecx+164]
push eax
call _fb_StrAssign@20
push 0
push -1
mov eax, dword ptr [ebp+12]
lea ecx, [eax+176]
push ecx
push -1
mov ecx, dword ptr [ebp+8]
lea eax, [ecx+176]
push eax
call _fb_StrAssign@20
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+188]
mov dword ptr [ecx+188], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+192]
mov dword ptr [ecx+192], eax
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+196]
mov dword ptr [ecx+196], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+200]
mov dword ptr [ecx+200], eax
mov eax, dword ptr [ebp+12]
lea ecx, [eax+208]
push ecx
mov ecx, dword ptr [ebp+8]
lea eax, [ecx+208]
push eax
call __ZN12_WORKER_VARSaSERS_
add esp, 8
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+264]
mov dword ptr [ecx+264], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+268]
mov dword ptr [ecx+268], eax
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+272]
mov dword ptr [ecx+272], ebx
.Lt_000C:
pop ebx
mov esp, ebp
pop ebp
ret
.balign 16
__ZN5_KGUIaSERS_:
push ebp
mov ebp, esp
sub esp, 24
push ebx
.Lt_0010:
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax]
mov dword ptr [ebx], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+4]
mov dword ptr [ebx+4], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+8]
mov dword ptr [ebx+8], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+12]
mov dword ptr [ebx+12], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+16]
mov dword ptr [ebx+16], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+20]
mov dword ptr [ebx+20], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+24]
mov dword ptr [ebx+24], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+28]
mov dword ptr [ebx+28], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+32]
mov dword ptr [ebx+32], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+36]
mov dword ptr [ebx+36], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+40]
mov dword ptr [ebx+40], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+44]
mov dword ptr [ebx+44], eax
push 0
push -1
mov eax, dword ptr [ebp+12]
lea ebx, [eax+48]
push ebx
push -1
mov ebx, dword ptr [ebp+8]
lea eax, [ebx+48]
push eax
call _fb_StrAssign@20
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+60]
mov dword ptr [ebx+60], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+64]
mov dword ptr [ebx+64], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+68]
mov dword ptr [ebx+68], ecx
mov ecx, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov eax, dword ptr [ecx+72]
mov dword ptr [ebx+72], eax
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [ebp+8]
mov ecx, dword ptr [eax+76]
mov dword ptr [ebx+76], ecx
push 0
push -1
mov ecx, dword ptr [ebp+12]
lea ebx, [ecx+80]
push ebx
push -1
mov ebx, dword ptr [ebp+8]
lea ecx, [ebx+80]
push ecx
call _fb_StrAssign@20
mov eax, dword ptr [ebp+8]
lea ecx, [eax+92]
mov dword ptr [ebp-8], ecx
mov ecx, dword ptr [ebp+12]
lea eax, [ecx+92]
mov dword ptr [ebp-12], eax
mov dword ptr [ebp-4], 0
.Lt_0013:
push 0
push -1
push dword ptr [ebp-12]
push -1
push dword ptr [ebp-8]
call _fb_StrAssign@20
add dword ptr [ebp-8], 12
add dword ptr [ebp-12], 12
inc dword ptr [ebp-4]
cmp dword ptr [ebp-4], 231
jne .Lt_0013
mov eax, dword ptr [ebp+8]
lea ecx, [eax+2864]
mov dword ptr [ebp-20], ecx
mov ecx, dword ptr [ebp+12]
lea eax, [ecx+2864]
mov dword ptr [ebp-24], eax
mov dword ptr [ebp-16], 0
.Lt_0017:
mov eax, dword ptr [ebp-24]
mov ecx, dword ptr [ebp-20]
mov ebx, dword ptr [eax]
mov dword ptr [ecx], ebx
add dword ptr [ebp-20], 4
add dword ptr [ebp-24], 4
inc dword ptr [ebp-16]
cmp dword ptr [ebp-16], 1001
jne .Lt_0017
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
movlpd xmm7, qword ptr [ebx+6872]
movlpd qword ptr [ecx+6872], xmm7
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+6880]
mov dword ptr [ecx+6880], eax
mov eax, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov ebx, dword ptr [eax+6884]
mov dword ptr [ecx+6884], ebx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+6888]
mov dword ptr [ecx+6888], eax
mov eax, dword ptr [ebp+8]
lea ecx, [eax+6892]
mov eax, dword ptr [ebp+12]
lea ebx, [eax+6892]
push ecx
push edi
push esi
mov edi, ecx
mov esi, ebx
mov ecx, 256
rep movsd
pop esi
pop edi
pop ecx
mov ebx, dword ptr [ebp+12]
mov ecx, dword ptr [ebp+8]
mov eax, dword ptr [ebx+7916]
mov dword ptr [ecx+7916], eax
.Lt_0011:
pop ebx
mov esp, ebp
pop ebp
ret
	#kwikpad.bas' compilation took 0.01896062823073663 secs

.section .data
.balign 4
_Lt_0000:	.ascii	"\0"
.balign 4
_Lt_000A:	.ascii	"*.*\0"
.balign 4
_Lt_000F:	.ascii	"0.6.7\0"

.section .bss
.balign 4
	.lcomm	_SELECTION,4

.section .data
.balign 4
_Lt_001D:	.ascii	"KwikPad\0"
.balign 4
_Lt_0021:	.ascii	"&File&&Open&&Save&&---------&&Quit\0"
.balign 4
_Lt_0022:	.ascii	"MainMenu\0"
.balign 4
_Lt_0024:	.ascii	"MultiText\0"
.balign 4
_Lt_0037:	.ascii	"File Open\0"
.balign 4
_Lt_0039:	.ascii	"OK\0"
.balign 4
_Lt_003B:	.ascii	"Cancel\0"
.balign 4
_Lt_003C:	.ascii	"CANCEL\0"
.balign 4
_Lt_003E:	.ascii	"kwikpad.bas\0"
.balign 4
_Lt_0068:	.ascii	"Open\0"
.balign 4
_Lt_006B:	.ascii	"Quit\0"
