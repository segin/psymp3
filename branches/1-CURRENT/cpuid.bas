/'
    $Id $

    This file is part of PsyMP3.

    PsyMP3 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    PsyMP3 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PsyMP3; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
'/

''
'' Start CPUID Functions.
''

#Include "cpuid.bi"

#Ifdef USE_ASM

#define true 1
#define false 0

Function ReadEFLAG Alias "ReadEFLAG" () as UInteger Export
	asm pushfd  'eflag on stack
	asm pop eax 'in eax
	asm mov [function],eax 
End Function

Sub WriteEFLAG Alias "WriteEFLAG" (ByVal value as UInteger) Export
	asm mov eax,[value]
	asm push eax 'value on stack
	asm popfd    'pop in eflag
End Sub

Function IsCPUID Alias "IsCPUID" () As Boolean Export 'CPUID command available
	dim as UInteger old, _new
	old = readeflag() 
	_new = old xor &H200000 'change bit 21
	writeeflag _new
	_new = readeflag()
	if old<>_new then
		function = true
		writeeflag old 'restore old value
	end if
end function

Function IsFPU486 Alias "IsFPU486" () As Boolean Export 'FPU available
	Dim tmp As UShort
	tmp = &HFFFF
	Asm fninit 'try FPU init
	Asm fnstsw [tmp] 'store statusword
	If tmp=0 Then 'is it really 0
		Asm fnstcw [tmp] 'store control
		If tmp=&H37F Then Function = TRUE
	End If
End Function

Function CPUCounter Alias "CPUCounter" () as ULongInt Export
	Dim tmp as ULongInt
	Asm RDTSC
	Asm mov [tmp],eax
	Asm mov [tmp+4],edx
	Function = tmp
End Function

'' Inspired by the Linux kernel

Function CPUID_EDX Alias "CPUID_EDX" (byval funcnr as uinteger) as UInteger Export
	asm mov dword ptr eax,[funcnr]
	asm cpuid
	asm mov [function],edx
end Function

Function CPUID_ECX Alias "CPUID_ECX" (byval funcnr as uinteger) as UInteger Export
	asm mov dword ptr eax,[funcnr]
	asm cpuid
	asm mov [function],ecx
end Function

Function CPUID_EBX Alias "CPUID_EBX" (byval funcnr as uinteger) as UInteger Export
	asm mov dword ptr eax,[funcnr]
	asm cpuid
	asm mov [function],ebx
end Function

Function CPUID_EAX Alias "CPUID_EAX" (byval funcnr as uinteger) as UInteger Export
	asm mov dword ptr eax,[funcnr]
	asm cpuid
	asm mov [function],eax
end Function

Sub CPUID Alias "CPUID" (byval funcnr as UInteger, ByVal ra As Any Ptr, ByVal rb As Any Ptr, ByVal rc As Any Ptr, ByVal rd As Any Ptr) Export 
	Asm 
		mov dword ptr eax,[funcnr]
		cpuid
		mov [ra],eax
		mov [rb],ebx
		mov [rc],ecx
		mov [rd],edx     
	End Asm
End Sub

Function GetCPUVendor Alias "GetCPUVendor" () As String Export
	Dim vendor As ZString * 12 
	Asm
		mov eax, 0x0
		cpuid
		mov [vendor+0], ebx
		mov [vendor+4], edx
		mov [vendor+8], ecx
	End Asm
	Return vendor
End Function

Function GetCPUName Alias "GetCPUName" () As String Export
	Dim cpu As ZString * 48
	Asm
	      mov eax, 0x80000002
	      cpuid
	      mov [cpu+0], eax
	      mov [cpu+4], ebx
	      mov [cpu+8], ecx
	      mov [cpu+12], edx
	      mov eax, 0x80000003
	      cpuid
	      mov [cpu+16], eax
	      mov [cpu+20], ebx
	      mov [cpu+24], ecx
	      mov [cpu+28], edx
	      mov eax, 0x80000004
	      cpuid
	      mov [cpu+32], eax
	      mov [cpu+36], ebx
	      mov [cpu+40], ecx
	      mov [cpu+44], edx      
	End Asm
	Return cpu
End Function

Function IsSSE2 Alias "IsSSE2" () As Boolean Export
	dim retvalue as uinteger
	retvalue = CPUID_EDX(1)
	If (retvalue And &H4000000) Then Return TRUE Else Return False
End Function

Function IsSSE Alias "IsSSE" () As Boolean Export
	dim retvalue as uinteger
	retvalue = CPUID_EDX(1)
	If (retvalue And &H2000000) Then Return TRUE Else Return False
End Function

Function GetMHz Alias "GetMHz" () As Integer Export
	Dim As UInteger retvalue, flag
	Dim As Double a, Start
	
	retvalue=CPUID_EDX(1)
	if (retvalue and &H10) then 
		flag=true
	end If
	
	if flag=true then
		a=CPUCounter():a-=8500000
		Start =timer()
		while (timer()-start)<1.0:wend
		a=CPUCounter()-a
		a\=1000000
		Return a
	end If
	Return 0
End Function

#EndIf

''
'' End CPUID Functions.
''
