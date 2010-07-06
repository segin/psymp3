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

#Ifndef __CPUID_BI__
#Define __CPUID_BI__

'#Include Once "psymp3.bi"

#Ifdef USE_ASM
Declare Function ReadEFLAG Alias "ReadEFLAG" () as UInteger
Declare Sub WriteEFLAG Alias "WriteEFLAG" (ByVal value as UInteger)
Declare Function IsCPUID Alias "IsCPUID" () As Boolean
Declare Function IsFPU486 Alias "IsFPU486" () As Boolean
Declare Function CPUCounter Alias "CPUCounter" () as ULongInt
Declare Function CPUID_EDX Alias "CPUID_EDX" (byval funcnr as uinteger) as UInteger
Declare Function CPUID_ECX Alias "CPUID_ECX" (byval funcnr as uinteger) as UInteger
Declare Function CPUID_EBX Alias "CPUID_EBX" (byval funcnr as uinteger) as UInteger
Declare Function CPUID_EAX Alias "CPUID_EAX" (byval funcnr as uinteger) as UInteger
Declare Sub CPUID Alias "CPUID" (byval funcnr as UInteger, ByVal ra As Any Ptr, ByVal rb As Any Ptr, ByVal rc As Any Ptr, ByVal rd As Any Ptr)
Declare Function GetCPUVendor Alias "GetCPUVendor" () As String
Declare Function GetCPUName Alias "GetCPUName" () As String
Declare Function IsSSE2 Alias "IsSSE2" () As Boolean
Declare Function IsSSE Alias "IsSSE" () As Boolean
Declare Function GetMHz Alias "GetMHz" () As Integer
#EndIf /' USE_ASM '/

#EndIf /' __CPUID_BI__ '/
