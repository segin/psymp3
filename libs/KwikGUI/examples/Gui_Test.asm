	.intel_syntax noprefix

	#Gui_Test.bas' compilation started at 09:23:58 (FreeBASIC v0.21.0b)

.section .text
.balign 16

.globl _MESSAGEHANDLER@8
_MESSAGEHANDLER@8:
push ebp
mov ebp, esp
sub esp, 68
push ebx
mov dword ptr [ebp-4], 0
.Lt_0083:
mov eax, dword ptr [ebp+12]
mov ebx, dword ptr [eax+4]
mov dword ptr [ebp-8], ebx
mov ebx, dword ptr [ebp+12]
mov eax, dword ptr [ebx]
mov dword ptr [ebp-12], eax
mov eax, dword ptr [ebp-8]
mov dword ptr [ebp-16], eax
mov eax, dword ptr [ebp+8]
mov dword ptr [ebp-20], eax
cmp dword ptr [ebp-20], 0
jne .Lt_0088
.Lt_0089:
jmp .Lt_0086
.Lt_0088:
cmp dword ptr [ebp-20], 1
jne .Lt_008A
.Lt_008B:
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
push dword ptr [eax+192]
call _fb_IntToStr@4
push eax
push 20
push offset _Lt_008C
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea eax, [ebp-32]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
push 9
push offset _Lt_003A
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+164]
push ebx
call _fb_StrCompare@16
test eax, eax
jne .Lt_0090
mov dword ptr [_SELECTION], 3
.Lt_0090:
.Lt_008F:
jmp .Lt_0086
.Lt_008A:
cmp dword ptr [ebp-20], 2
jne .Lt_0091
.Lt_0092:
push 9
push offset _Lt_0060
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+164]
push ebx
call _fb_StrCompare@16
test eax, eax
jne .Lt_0094
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+176]
push ebx
push 15
push offset _Lt_0095
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea ebx, [ebp-32]
push ebx
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
mov dword ptr [_SELECTION], 5
.Lt_0094:
.Lt_0093:
jmp .Lt_0086
.Lt_0091:
cmp dword ptr [ebp-20], 3
jne .Lt_0098
.Lt_0099:
mov eax, dword ptr [ebp+12]
cmp dword ptr [eax+8], 2
jne .Lt_009B
mov eax, dword ptr [ebp+12]
push dword ptr [eax+16]
mov eax, dword ptr [ebp+12]
push dword ptr [eax+12]
push 0
push dword ptr [ebp-12]
call __ZN5_KGUI13ACTIVATEPOPUPEiii@16
.Lt_009B:
.Lt_009A:
jmp .Lt_0086
.Lt_0098:
cmp dword ptr [ebp-20], 4
jne .Lt_009C
.Lt_009D:
push 8
push offset _Lt_0052
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+164]
push ebx
call _fb_StrCompare@16
test eax, eax
sete al
shr eax, 1
sbb eax, eax
push 8
push offset _Lt_0054
push -1
mov ebx, dword ptr [ebp-16]
lea ecx, [ebx+164]
push ecx
mov ebx, eax
call _fb_StrCompare@16
test eax, eax
sete al
shr eax, 1
sbb eax, eax
or ebx, eax
je .Lt_009F
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
push dword ptr [eax+192]
call _fb_IntToStr@4
push eax
push 14
push offset _Lt_00A0
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea eax, [ebp-32]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
.Lt_009F:
.Lt_009E:
jmp .Lt_0086
.Lt_009C:
cmp dword ptr [ebp-20], 5
jne .Lt_00A3
.Lt_00A4:
jmp .Lt_0086
.Lt_00A3:
cmp dword ptr [ebp-20], 6
jne .Lt_00A5
.Lt_00A6:
jmp .Lt_0086
.Lt_00A5:
cmp dword ptr [ebp-20], 8
jne .Lt_00A7
.Lt_00A8:
push 0
push -1
call _fb_Time@0
push eax
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [_PROGRESS]
inc dword ptr [eax]
mov eax, dword ptr [_PROGRESS]
cmp dword ptr [eax], 100
jle .Lt_00AA
mov eax, dword ptr [_PROGRESS]
mov dword ptr [eax], 0
.Lt_00AA:
jmp .Lt_0086
.Lt_00A7:
cmp dword ptr [ebp-20], 9
jne .Lt_00AB
.Lt_00AC:
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+128]
push ebx
push 15
push offset _Lt_00AD
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea ebx, [ebp-32]
push ebx
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
push 5
push offset _Lt_00B0
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+128]
push ebx
call _fb_StrCompare@16
test eax, eax
jne .Lt_00B2
mov dword ptr [_SELECTION], 1
.Lt_00B2:
.Lt_00B1:
jmp .Lt_0086
.Lt_00AB:
cmp dword ptr [ebp-20], 10
jne .Lt_00B3
.Lt_00B4:
mov dword ptr [ebp-68], 0
mov dword ptr [ebp-64], 0
mov dword ptr [ebp-60], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
push dword ptr [eax+68]
call _fb_IntToStr@4
push eax
push -1
push 2
push offset _Lt_00B6
push -1
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+176]
push ebx
push 17
push offset _Lt_00B5
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea ebx, [ebp-32]
push ebx
call _fb_StrConcat@20
push eax
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
lea eax, [ebp-44]
push eax
call _fb_StrConcat@20
push eax
mov dword ptr [ebp-56], 0
mov dword ptr [ebp-52], 0
mov dword ptr [ebp-48], 0
lea eax, [ebp-56]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-68]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-68]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-68]
push eax
call _fb_StrDelete@4
jmp .Lt_0086
.Lt_00B3:
cmp dword ptr [ebp-20], 11
jne .Lt_00BB
.Lt_00BC:
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp-16]
push dword ptr [eax+192]
call _fb_IntToStr@4
push eax
push 14
push offset _Lt_00A0
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea eax, [ebp-32]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
mov dword ptr [_SELECTION], 4
jmp .Lt_0086
.Lt_00BB:
cmp dword ptr [ebp-20], 12
jne .Lt_00BF
.Lt_00C0:
push 8
push offset _Lt_005E
push -1
mov eax, dword ptr [ebp-16]
lea ebx, [eax+164]
push ebx
call _fb_StrCompare@16
test eax, eax
jne .Lt_00C2
mov dword ptr [_SELECTION], 2
.Lt_00C2:
.Lt_00C1:
jmp .Lt_0086
.Lt_00BF:
cmp dword ptr [ebp-20], 13
jne .Lt_00C3
.Lt_00C4:
mov dword ptr [ebp-44], 0
mov dword ptr [ebp-40], 0
mov dword ptr [ebp-36], 0
push 0
push -1
push -1
mov eax, dword ptr [ebp+12]
push dword ptr [eax+20]
call _fb_IntToStr@4
push eax
push 11
push offset _Lt_00C5
mov dword ptr [ebp-32], 0
mov dword ptr [ebp-28], 0
mov dword ptr [ebp-24], 0
lea eax, [ebp-32]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-44]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-44]
push dword ptr [ebp-12]
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-44]
push eax
call _fb_StrDelete@4
.Lt_00C3:
.Lt_0086:
mov dword ptr [ebp-4], 0
.Lt_0084:
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
sub esp, 13652
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
push 600
push 800
call _fb_GfxScreenRes@24
push 600
push 800
push 0
push 0
lea eax, [ebp-7924]
push eax
call __ZN5_KGUIC1Eiiii
add esp, 20
push -1
lea eax, [ebp-7876]
push eax
push 13
push offset _Lt_001D
mov dword ptr [ebp-7936], 0
mov dword ptr [ebp-7932], 0
mov dword ptr [ebp-7928], 0
lea eax, [ebp-7936]
push eax
call _fb_StrConcat@20
push eax
call _fb_GfxSetWindowTitle@4
mov dword ptr [ebp-7904], 170
mov dword ptr [ebp-7916], 12632256
push offset _MESSAGEHANDLER@8
push 13
lea eax, [ebp-8216]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 4
lea eax, [ebp-8496]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 0
lea eax, [ebp-8776]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 2
lea eax, [ebp-9056]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 1
lea eax, [ebp-9336]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 5
lea eax, [ebp-9616]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 6
lea eax, [ebp-9896]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 7
lea eax, [ebp-10176]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 16
lea eax, [ebp-10456]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 15
lea eax, [ebp-10736]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 14
lea eax, [ebp-11016]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 9
lea eax, [ebp-11296]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 17
lea eax, [ebp-11576]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 18
lea eax, [ebp-11856]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 11
lea eax, [ebp-12136]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 10
lea eax, [ebp-12416]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 19
lea eax, [ebp-12696]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 21
lea eax, [ebp-12976]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 21
lea eax, [ebp-13256]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
push offset _MESSAGEHANDLER@8
push 21
lea eax, [ebp-13536]
push eax
call __ZN13_KGUI_CONTROLC1E10_CTRL_TYPEPv
add esp, 12
lea eax, [ebp-8216]
mov dword ptr [ebp-13540], eax
mov eax, dword ptr [ebp-13540]
mov dword ptr [eax+12], 0
mov eax, dword ptr [ebp-13540]
mov dword ptr [eax+16], 0
mov eax, dword ptr [ebp-13540]
mov dword ptr [eax+20], 800
mov eax, dword ptr [ebp-13540]
mov dword ptr [eax+24], 600
lea eax, [ebp-8496]
mov dword ptr [ebp-13544], eax
push offset _Lt_0035
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10LOADBITMAPE8FBSTRING@8
mov dword ptr [ebp-13548], eax
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+12], 40
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+16], 50
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+20], 48
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+24], 48
push dword ptr [ebp-13548]
push dword ptr [ebp-13544]
call __ZN13_KGUI_CONTROL12SETMASKIMAGEEPv@8
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+44], 16777214
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+32], 16716066
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+64], 2
push 0
push 4
push offset _Lt_0036
push -1
mov eax, dword ptr [ebp-13544]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 12
push offset _Lt_0037
push -1
mov eax, dword ptr [ebp-13544]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13544]
mov dword ptr [eax+188], 0
lea eax, [ebp-8496]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-9056]
mov dword ptr [ebp-13552], eax
mov eax, dword ptr [ebp-13552]
mov dword ptr [eax+12], 20
mov eax, dword ptr [ebp-13552]
mov dword ptr [eax+16], 120
mov eax, dword ptr [ebp-13552]
mov dword ptr [eax+144], 10
push 0
push 13
push offset _Lt_0039
push -1
mov eax, dword ptr [ebp-13552]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13552]
mov dword ptr [eax+32], 16777215
mov eax, dword ptr [ebp-13552]
mov dword ptr [eax+44], 255
push 0
push 9
push offset _Lt_003A
push -1
mov eax, dword ptr [ebp-13552]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-9056]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-8776]
mov dword ptr [ebp-13556], eax
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+12], 30
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+16], 145
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+20], 75
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+24], 20
push 0
push 6
push offset _Lt_003C
push -1
mov eax, dword ptr [ebp-13556]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+28], 1
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+64], 2
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+188], 1
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+32], 12632256
mov eax, dword ptr [ebp-13556]
mov dword ptr [eax+44], 0
push 0
push 11
push offset _Lt_003D
push -1
mov eax, dword ptr [ebp-13556]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-8776]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-9336]
mov dword ptr [ebp-13560], eax
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+12], 30
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+16], 190
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+20], 100
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+24], 16
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+64], 2
push 0
push 8
push offset _Lt_003F
push -1
mov eax, dword ptr [ebp-13560]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+32], 16777215
mov eax, dword ptr [ebp-13560]
mov dword ptr [eax+44], 0
push 0
push 9
push offset _Lt_0040
push -1
mov eax, dword ptr [ebp-13560]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-9336]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-9616]
mov dword ptr [ebp-13564], eax
push offset _Lt_0042
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10LOADBITMAPE8FBSTRING@8
mov dword ptr [ebp-13568], eax
mov eax, dword ptr [ebp-13564]
mov dword ptr [eax+12], 45
mov eax, dword ptr [ebp-13564]
mov dword ptr [eax+16], 230
mov eax, dword ptr [ebp-13564]
mov dword ptr [eax+20], 48
mov eax, dword ptr [ebp-13564]
mov dword ptr [eax+24], 48
mov eax, dword ptr [ebp-13564]
mov dword ptr [eax+28], 0
mov eax, dword ptr [ebp-13564]
mov ebx, dword ptr [ebp-13568]
mov dword ptr [eax+108], ebx
mov ebx, dword ptr [ebp-13564]
mov dword ptr [ebx+188], 1
push 0
push 12
push offset _Lt_0043
push -1
mov ebx, dword ptr [ebp-13564]
lea eax, [ebx+164]
push eax
call _fb_StrAssign@20
lea eax, [ebp-9616]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-9896]
mov dword ptr [ebp-13572], eax
mov eax, dword ptr [ebp-13572]
movlpd xmm7, qword ptr [_Lt_00CA]
movlpd qword ptr [eax+56], xmm7
push 0
push 14
push offset _Lt_0045
push -1
mov eax, dword ptr [ebp-13572]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-9896]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-10176]
mov dword ptr [ebp-13576], eax
push 0
push 66
push offset _Lt_004C
push -1
mov eax, dword ptr [ebp-13576]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 15
push offset _Lt_004D
push -1
mov eax, dword ptr [ebp-13576]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13576]
mov dword ptr [eax+32], 6689041
mov eax, dword ptr [ebp-13576]
mov dword ptr [eax+44], 16777215
mov eax, dword ptr [ebp-13576]
mov dword ptr [eax+152], 0
lea eax, [ebp-10176]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-10456]
mov dword ptr [ebp-13580], eax
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+12], 150
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+16], 50
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+20], 640
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+24], 250
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+32], 14737632
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+64], 2
push 0
push -1
push offset _Lt_004F
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI12LOADTEXTFILEE8FBSTRING@8
push eax
push -1
mov eax, dword ptr [ebp-13580]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
push 0
push 10
push offset _Lt_0050
push -1
mov eax, dword ptr [ebp-13580]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+28], 0
mov eax, dword ptr [ebp-13580]
mov dword ptr [eax+116], 1
lea eax, [ebp-10456]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-10736]
mov dword ptr [ebp-13584], eax
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+12], 100
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+16], 220
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+20], 15
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+24], 100
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+76], 0
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+80], 255
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+192], 255
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+28], 1
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+144], 5
mov eax, dword ptr [ebp-13584]
mov dword ptr [eax+44], 65280
push 0
push 8
push offset _Lt_0052
push -1
mov eax, dword ptr [ebp-13584]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-10736]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-11016]
mov dword ptr [ebp-13588], eax
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+12], 20
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+16], 340
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+20], 100
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+24], 15
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+76], 0
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+80], 100
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+28], 1
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+144], 5
mov eax, dword ptr [ebp-13588]
mov dword ptr [eax+44], 16776960
push 0
push 8
push offset _Lt_0054
push -1
mov eax, dword ptr [ebp-13588]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-11016]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-11296]
mov dword ptr [ebp-13592], eax
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+12], 20
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+16], 370
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+20], 120
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+24], 12
push 0
push 6
push offset _Lt_0056
push -1
mov eax, dword ptr [ebp-13592]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+28], 1
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+64], 0
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+188], 0
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+32], 14737632
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+44], 0
push 0
push 6
push offset _Lt_0056
push -1
mov eax, dword ptr [ebp-13592]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+152], 1
mov eax, dword ptr [ebp-13592]
mov dword ptr [eax+116], 1
lea eax, [ebp-11296]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-11576]
mov dword ptr [ebp-13596], eax
mov eax, dword ptr [ebp-13596]
mov dword ptr [eax+20], 120
mov eax, dword ptr [ebp-13596]
mov dword ptr [eax+64], 0
mov eax, dword ptr [ebp-13596]
mov dword ptr [eax+32], 16776960
push 0
push offset _Lt_0058
push dword ptr [ebp-13596]
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
push 0
push offset _Lt_0059
push dword ptr [ebp-13596]
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
push 0
push offset _Lt_005A
push dword ptr [ebp-13596]
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
push 0
push offset _Lt_005B
push dword ptr [ebp-13596]
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
lea eax, [ebp-11576]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-11856]
mov dword ptr [ebp-13600], eax
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+12], 250
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+16], 560
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+20], 300
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+24], 15
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+64], 2
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+44], 39168
mov eax, dword ptr [ebp-13600]
mov dword ptr [eax+32], 0
lea eax, [ebp-11664]
mov dword ptr [_PROGRESS], eax
lea eax, [ebp-11856]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-12136]
mov dword ptr [ebp-13604], eax
mov eax, dword ptr [ebp-13604]
mov dword ptr [eax+12], 200
mov eax, dword ptr [ebp-13604]
mov dword ptr [eax+16], 350
mov eax, dword ptr [ebp-13604]
mov dword ptr [eax+20], 150
mov eax, dword ptr [ebp-13604]
mov dword ptr [eax+24], 150
mov eax, dword ptr [ebp-13604]
mov dword ptr [eax+32], 16776960
push 0
push -1
call _fb_CurDir@0
push eax
push -1
mov eax, dword ptr [ebp-13604]
lea ebx, [eax+84]
push ebx
call _fb_StrAssign@20
push 0
push 8
push offset _Lt_005E
push -1
mov eax, dword ptr [ebp-13604]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
push dword ptr [ebp-13604]
call __ZN13_KGUI_CONTROL7REFRESHEv@4
lea eax, [ebp-12136]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-12416]
mov dword ptr [ebp-13608], eax
mov eax, dword ptr [ebp-13608]
mov dword ptr [eax+12], 400
mov eax, dword ptr [ebp-13608]
mov dword ptr [eax+16], 350
mov eax, dword ptr [ebp-13608]
mov dword ptr [eax+20], 150
mov eax, dword ptr [ebp-13608]
mov dword ptr [eax+24], 150
mov eax, dword ptr [ebp-13608]
mov dword ptr [eax+32], 16776960
push 0
push -1
call _fb_CurDir@0
push eax
push -1
mov eax, dword ptr [ebp-13608]
lea ebx, [eax+84]
push ebx
call _fb_StrAssign@20
push 0
push 4
push offset _Lt_000A
push -1
mov eax, dword ptr [ebp-13608]
lea ebx, [eax+96]
push ebx
call _fb_StrAssign@20
push 0
push 9
push offset _Lt_0060
push -1
mov eax, dword ptr [ebp-13608]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
push dword ptr [ebp-13608]
call __ZN13_KGUI_CONTROL7REFRESHEv@4
lea eax, [ebp-12416]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-12696]
mov dword ptr [ebp-13612], eax
mov eax, dword ptr [ebp-13612]
mov dword ptr [eax+12], 600
mov eax, dword ptr [ebp-13612]
mov dword ptr [eax+16], 350
mov eax, dword ptr [ebp-13612]
mov dword ptr [eax+20], 150
mov eax, dword ptr [ebp-13612]
mov dword ptr [eax+24], 150
mov eax, dword ptr [ebp-13612]
mov dword ptr [eax+32], 16777215
push 0
push 8
push offset _Lt_0062
push -1
mov eax, dword ptr [ebp-13612]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
push dword ptr [ebp-13612]
call __ZN13_KGUI_CONTROL7REFRESHEv@4
lea eax, [ebp-12696]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-12976]
mov dword ptr [ebp-13616], eax
mov eax, dword ptr [ebp-13616]
mov dword ptr [eax+12], 25
mov eax, dword ptr [ebp-13616]
mov dword ptr [eax+16], 500
push 0
push 6
push offset _Lt_0064
push -1
mov eax, dword ptr [ebp-13616]
lea ebx, [eax+164]
push ebx
call _fb_StrAssign@20
push 0
push 13
push offset _Lt_0065
push -1
mov eax, dword ptr [ebp-13616]
lea ebx, [eax+176]
push ebx
call _fb_StrAssign@20
lea eax, [ebp-13256]
push eax
lea eax, [ebp-12976]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI14COPYPROPERTIESEP13_KGUI_CONTROLS1_@12
lea eax, [ebp-13536]
push eax
lea eax, [ebp-12976]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI14COPYPROPERTIESEP13_KGUI_CONTROLS1_@12
mov dword ptr [ebp-13240], 520
mov dword ptr [ebp-13520], 540
push 0
push 13
push offset _Lt_0066
push -1
lea eax, [ebp-13080]
push eax
call _fb_StrAssign@20
push 0
push 13
push offset _Lt_0067
push -1
lea eax, [ebp-13360]
push eax
call _fb_StrAssign@20
lea eax, [ebp-12976]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-13256]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
lea eax, [ebp-13536]
push eax
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI10ADDCONTROLEP13_KGUI_CONTROL@8
mov dword ptr [ebp-13652], 0
mov dword ptr [ebp-13648], 0
mov dword ptr [ebp-13644], 0
push 0
push -1
push 0
push 33
push 3
push offset _Lt_000A
call _fb_StrAllocTempDescZEx@8
push eax
call _fb_Dir@12
push eax
push -1
lea eax, [ebp-13652]
push eax
call _fb_StrAssign@20
.Lt_0068:
push -1
lea eax, [ebp-13652]
push eax
call _fb_StrLen@8
test eax, eax
je .Lt_0069
push 0
push dword ptr [ebp-13652]
lea eax, [ebp-11296]
push eax
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
push 0
push -1
push 0
call _fb_DirNext@4
push eax
push -1
lea eax, [ebp-13652]
push eax
call _fb_StrAssign@20
jmp .Lt_0068
.Lt_0069:
lea eax, [ebp-13652]
push eax
call _fb_StrDelete@4
mov dword ptr [ebp-13640], 0
mov dword ptr [ebp-13636], 0
mov dword ptr [ebp-13632], 0
push 0
push -1
push -1
lea eax, [ebp-11296]
push eax
call __ZN13_KGUI_CONTROL12GETITEMCOUNTEv@4
push eax
call _fb_IntToStr@4
push eax
push 13
push offset _Lt_006A
mov dword ptr [ebp-13628], 0
mov dword ptr [ebp-13624], 0
mov dword ptr [ebp-13620], 0
lea eax, [ebp-13628]
push eax
call _fb_StrConcat@20
push eax
push -1
lea eax, [ebp-13640]
push eax
call _fb_StrAssign@20
push dword ptr [ebp-13640]
lea eax, [ebp-7924]
push eax
call __ZN5_KGUI7CONSOLEE8FBSTRING@8
lea eax, [ebp-13640]
push eax
call _fb_StrDelete@4
push 0
push 7
push offset _Lt_006D
push -1
lea eax, [ebp-10000]
push eax
call _fb_StrAssign@20
lea eax, [ebp-10176]
push eax
call __ZN13_KGUI_CONTROL7REFRESHEv@4
.Lt_006E:
mov dword ptr [_SELECTION], 0
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
push 0
push -1
lea eax, [ebp-9720]
push eax
push -1
lea eax, [ebp-8600]
push eax
call _fb_StrAssign@20
push 0
push 9
push offset _Lt_0071
push -1
lea eax, [ebp-11680]
push eax
call _fb_StrAssign@20
push 0
push -1
push dword ptr [ebp-11664]
call _fb_IntToStr@4
push eax
push -1
lea eax, [ebp-11680]
push eax
call _fb_StrConcatAssign@20
push 0
push 2
push offset _Lt_0072
push -1
lea eax, [ebp-11680]
push eax
call _fb_StrConcatAssign@20
push 1
call _fb_Multikey@4
cmp eax, -1
jne .Lt_0074
mov dword ptr [_SELECTION], 1
.Lt_0074:
.Lt_0073:
mov eax, dword ptr [_SELECTION]
mov dword ptr [ebp-13644], eax
cmp dword ptr [ebp-13644], 1
jne .Lt_0077
.Lt_0078:
jmp .Lt_006F
jmp .Lt_0075
.Lt_0077:
cmp dword ptr [ebp-13644], 2
jne .Lt_0079
.Lt_007A:
push 0
push -1
lea eax, [ebp-12052]
push eax
push -1
lea eax, [ebp-12332]
push eax
call _fb_StrAssign@20
lea eax, [ebp-12416]
push eax
call __ZN13_KGUI_CONTROL7REFRESHEv@4
jmp .Lt_0075
.Lt_0079:
cmp dword ptr [ebp-13644], 3
jne .Lt_007B
.Lt_007C:
cmp dword ptr [ebp-8864], 1
jne .Lt_007E
push 0
lea eax, [ebp-8496]
push eax
call __ZN13_KGUI_CONTROL12SETMASKIMAGEEPv@8
jmp .Lt_007D
.Lt_007E:
push dword ptr [ebp-13548]
lea eax, [ebp-8496]
push eax
call __ZN13_KGUI_CONTROL12SETMASKIMAGEEPv@8
.Lt_007D:
jmp .Lt_0075
.Lt_007B:
cmp dword ptr [ebp-13644], 4
jne .Lt_007F
.Lt_0080:
mov eax, dword ptr [ebp-10544]
mov dword ptr [ebp-9608], eax
jmp .Lt_0075
.Lt_007F:
cmp dword ptr [ebp-13644], 5
jne .Lt_0081
.Lt_0082:
push 0
push dword ptr [ebp-12240]
lea eax, [ebp-12696]
push eax
call __ZN13_KGUI_CONTROL7ADDITEME8FBSTRINGi@12
.Lt_0081:
.Lt_0075:
push 3
call _fb_Sleep@4
.Lt_0070:
jmp .Lt_006E
.Lt_006F:
lea eax, [ebp-13536]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-13256]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-12976]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-12696]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-12416]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-12136]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-11856]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-11576]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-11296]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-11016]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-10736]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-10456]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-10176]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-9896]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-9616]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-9336]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-9056]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8776]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8496]
push eax
call __ZN13_KGUI_CONTROLD1Ev
add esp, 4
lea eax, [ebp-8216]
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
	#Gui_Test.bas' compilation took 0.02411557346079007 secs

.section .data
.balign 4
_Lt_000A:	.ascii	"*.*\0"
.balign 4
_Lt_000F:	.ascii	"0.6.8\0"

.section .bss
.balign 4
	.lcomm	_SELECTION,4
.balign 4
	.lcomm	_PROGRESS,4

.section .data
.balign 4
_Lt_001D:	.ascii	"KwikGUI ver \0"
.balign 4
_Lt_0035:	.ascii	"btnmask.bmp\0"
.balign 4
_Lt_0036:	.ascii	"Btn\0"
.balign 4
_Lt_0037:	.ascii	"Command Btn\0"
.balign 4
_Lt_0039:	.ascii	"Button Style\0"
.balign 4
_Lt_003A:	.ascii	"CheckBox\0"
.balign 4
_Lt_003C:	.ascii	"Label\0"
.balign 4
_Lt_003D:	.ascii	"Text Label\0"
.balign 4
_Lt_003F:	.ascii	"TextBox\0"
.balign 4
_Lt_0040:	.ascii	"Text Box\0"
.balign 4
_Lt_0042:	.ascii	"test.bmp\0"
.balign 4
_Lt_0043:	.ascii	"Picture Box\0"
.balign 4
_Lt_0045:	.ascii	"Timer Control\0"
.balign 4
_Lt_004C:	.ascii	"&File&&Open&&Print&&Save&&---------&&Quit&Edit&&Undo&Format&&Bold\0"
.balign 4
_Lt_004D:	.ascii	"Drop Down Menu\0"
.balign 4
_Lt_004F:	.ascii	"GUI_Test.bas\0"
.balign 4
_Lt_0050:	.ascii	"MultiText\0"
.balign 4
_Lt_0052:	.ascii	"VSLIDER\0"
.balign 4
_Lt_0054:	.ascii	"HSLIDER\0"
.balign 4
_Lt_0056:	.ascii	"Combo\0"
.balign 4
_Lt_0058:	.ascii	"This One\0"
.balign 4
_Lt_0059:	.ascii	"Another One\0"
.balign 4
_Lt_005A:	.ascii	"Third One\0"
.balign 4
_Lt_005B:	.ascii	"Last One\0"
.balign 4
_Lt_005E:	.ascii	"DIRLIST\0"
.balign 4
_Lt_0060:	.ascii	"FILELIST\0"
.balign 4
_Lt_0062:	.ascii	"LISTBOX\0"
.balign 4
_Lt_0064:	.ascii	"RADIO\0"
.balign 4
_Lt_0065:	.ascii	"Selection #1\0"
.balign 4
_Lt_0066:	.ascii	"Selection #2\0"
.balign 4
_Lt_0067:	.ascii	"Selection #3\0"
.balign 4
_Lt_006A:	.ascii	"Total Items=\0"
.balign 4
_Lt_006D:	.ascii	"&Vince\0"
.balign 4
_Lt_0071:	.ascii	"Loading \0"
.balign 4
_Lt_0072:	.ascii	"%\0"
.balign 4
_Lt_008C:	.ascii	"Mouse_Click->value=\0"
.balign 4
_Lt_0095:	.ascii	"File Selected=\0"
.balign 4
_Lt_00A0:	.ascii	"Slider Value=\0"
.balign 4
_Lt_00AD:	.ascii	"Menu_Select ->\0"
.balign 4
_Lt_00B0:	.ascii	"Quit\0"
.balign 4
_Lt_00B5:	.ascii	"Combo Selection=\0"
.balign 4
_Lt_00B6:	.ascii	",\0"
.balign 4
_Lt_00C5:	.ascii	"Key_Press \0"
.balign 8
_Lt_00CA:	.double	0.01
