/*
 * third_party_licenses.h - generated; do not edit.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The licence texts below belong to their respective projects and are
 * reproduced verbatim. Regenerate with scripts/gen-third-party-licenses.sh
 * after editing THIRD-PARTY-LICENSES.txt, which is the source of truth.
 *
 * Several of the licences PsyMP3 links under -- zlib, BSD, MIT, curl,
 * Apache-2.0, the FreeType Licence and the Fraunhofer FDK AAC licence --
 * require their text, not merely a copyright line, to accompany a binary
 * redistribution. Embedding it here is what lets a lone .exe satisfy that.
 */

#ifndef PSYMP3_THIRD_PARTY_LICENSES_H
#define PSYMP3_THIRD_PARTY_LICENSES_H

/* The leading "\n" is the blank line that separates this from whatever
 * about.cpp concatenates in front of it. */
#define PSYMP3_THIRD_PARTY_LICENSES \
    "\n" \
    "PsyMP3 -- Third-Party Licenses\n" \
    "==============================\n" \
    "\n" \
    "PsyMP3 itself is distributed under the ISC License, reproduced first below.\n" \
    "\n" \
    "The binary distributions of PsyMP3 statically link or embed the third-party\n" \
    "components listed here, so their licenses travel with the executable. Every\n" \
    "license text below is reproduced from the upstream project word for word; the\n" \
    "only change is typographic, \"(c)\" and \"(C)\" in copyright notices being set as\n" \
    "the copyright sign. Section references such as Apache-2.0 4(c) are untouched.\n" \
    "\n" \
    "Where a component is offered under a choice of licenses, the license PsyMP3\n" \
    "elects is stated and only that one is reproduced:\n" \
    "\n" \
    "  * TagLib   -- offered under LGPL-2.1 or MPL-1.1; PsyMP3 elects MPL-1.1.\n" \
    "  * FreeType -- offered under the FreeType License or GPLv2; PsyMP3 elects\n" \
    "                the FreeType License (FTL).\n" \
    "  * stb_vorbis -- offered as public domain or MIT; PsyMP3 elects the public\n" \
    "                domain dedication.\n" \
    "\n" \
    "Note on patents: the Fraunhofer FDK AAC license below grants copyright\n" \
    "permissions only and expressly grants no patent license. See section 3 of\n" \
    "that license.\n" \
    "\n" \
    "Contents\n" \
    "--------\n" \
    "  1. PsyMP3                                  ISC License\n" \
    "  2. Apache License 2.0                      (ALAC, MLP/TrueHD, SheenBidi, OpenSSL)\n" \
    "  3. SDL3                                    zlib License\n" \
    "  4. zlib                                    zlib License\n" \
    "  5. kjmp2                                   zlib License\n" \
    "  6. FreeType                                FreeType License (FTL)\n" \
    "  7. HarfBuzz                                \"Old MIT\" License\n" \
    "  8. TagLib                                  Mozilla Public License 1.1\n" \
    "  9. FDK-AAC                                 Fraunhofer FDK AAC license\n" \
    " 10. libogg                                  BSD 3-Clause\n" \
    " 11. Opus                                    BSD 3-Clause\n" \
    " 12. Speex                                   BSD 3-Clause\n" \
    " 13. libcurl                                 curl License\n" \
    " 14. pugixml                                 MIT License\n" \
    " 15. stb_vorbis                              Public Domain (Unlicense)\n" \
    " 16. minimp3                                 CC0 / Public Domain\n" \
    " 17. DejaVu Sans (embedded UI font)          Bitstream Vera / Arev\n" \
    "\n" \
    "==============================================================================\n" \
    "1. PsyMP3 -- ISC License\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright © 2009-2026 Kirn Gill II <segin2005@gmail.com>\n" \
    "Copyright © 2010-2026 Mattis Michel <sic_zer0@hotmail.com>\n" \
    "Copyright © 2009-2026 Rajesh Rajan <seanawake@gmail.com>\n" \
    "\n" \
    "Permission to use, copy, modify, and/or distribute this software for any\n" \
    "purpose with or without fee is hereby granted, provided that the above\n" \
    "copyright notice and this permission notice appear in all copies.\n" \
    "\n" \
    "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH\n" \
    "REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY\n" \
    "AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,\n" \
    "INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM\n" \
    "LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR\n" \
    "OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n" \
    "PERFORMANCE OF THIS SOFTWARE.\n" \
    "\n" \
    "==============================================================================\n" \
    "2. Apache License 2.0 -- ALAC, MLP/TrueHD decoder, SheenBidi, OpenSSL\n" \
    "==============================================================================\n" \
    "\n" \
    "Applies to:\n" \
    "  * Apple ALAC decoder (third_party/alac)\n" \
    "      Copyright © 2011 Apple Inc. All rights reserved.\n" \
    "  * MLP/Dolby TrueHD decoder (third_party/mlp), derived from truehdd\n" \
    "      Copyright © 2025 Rainbaby\n" \
    "      Copyright © 2026 Kirn Gill II\n" \
    "  * OpenSSL\n" \
    "      Copyright © 2014-2026 Muhammad Tayyab Akram (SheenBidi)\n" \
    "      Copyright © 1998-2026 The OpenSSL Project Authors\n" \
    "\n" \
    "\n" \
    "                                 Apache License\n" \
    "                           Version 2.0, January 2004\n" \
    "                        https://www.apache.org/licenses/\n" \
    "\n" \
    "   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION\n" \
    "\n" \
    "   1. Definitions.\n" \
    "\n" \
    "      \"License\" shall mean the terms and conditions for use, reproduction,\n" \
    "      and distribution as defined by Sections 1 through 9 of this document.\n" \
    "\n" \
    "      \"Licensor\" shall mean the copyright owner or entity authorized by\n" \
    "      the copyright owner that is granting the License.\n" \
    "\n" \
    "      \"Legal Entity\" shall mean the union of the acting entity and all\n" \
    "      other entities that control, are controlled by, or are under common\n" \
    "      control with that entity. For the purposes of this definition,\n" \
    "      \"control\" means (i) the power, direct or indirect, to cause the\n" \
    "      direction or management of such entity, whether by contract or\n" \
    "      otherwise, or (ii) ownership of fifty percent (50%) or more of the\n" \
    "      outstanding shares, or (iii) beneficial ownership of such entity.\n" \
    "\n" \
    "      \"You\" (or \"Your\") shall mean an individual or Legal Entity\n" \
    "      exercising permissions granted by this License.\n" \
    "\n" \
    "      \"Source\" form shall mean the preferred form for making modifications,\n" \
    "      including but not limited to software source code, documentation\n" \
    "      source, and configuration files.\n" \
    "\n" \
    "      \"Object\" form shall mean any form resulting from mechanical\n" \
    "      transformation or translation of a Source form, including but\n" \
    "      not limited to compiled object code, generated documentation,\n" \
    "      and conversions to other media types.\n" \
    "\n" \
    "      \"Work\" shall mean the work of authorship, whether in Source or\n" \
    "      Object form, made available under the License, as indicated by a\n" \
    "      copyright notice that is included in or attached to the work\n" \
    "      (an example is provided in the Appendix below).\n" \
    "\n" \
    "      \"Derivative Works\" shall mean any work, whether in Source or Object\n" \
    "      form, that is based on (or derived from) the Work and for which the\n" \
    "      editorial revisions, annotations, elaborations, or other modifications\n" \
    "      represent, as a whole, an original work of authorship. For the purposes\n" \
    "      of this License, Derivative Works shall not include works that remain\n" \
    "      separable from, or merely link (or bind by name) to the interfaces of,\n" \
    "      the Work and Derivative Works thereof.\n" \
    "\n" \
    "      \"Contribution\" shall mean any work of authorship, including\n" \
    "      the original version of the Work and any modifications or additions\n" \
    "      to that Work or Derivative Works thereof, that is intentionally\n" \
    "      submitted to Licensor for inclusion in the Work by the copyright owner\n" \
    "      or by an individual or Legal Entity authorized to submit on behalf of\n" \
    "      the copyright owner. For the purposes of this definition, \"submitted\"\n" \
    "      means any form of electronic, verbal, or written communication sent\n" \
    "      to the Licensor or its representatives, including but not limited to\n" \
    "      communication on electronic mailing lists, source code control systems,\n" \
    "      and issue tracking systems that are managed by, or on behalf of, the\n" \
    "      Licensor for the purpose of discussing and improving the Work, but\n" \
    "      excluding communication that is conspicuously marked or otherwise\n" \
    "      designated in writing by the copyright owner as \"Not a Contribution.\"\n" \
    "\n" \
    "      \"Contributor\" shall mean Licensor and any individual or Legal Entity\n" \
    "      on behalf of whom a Contribution has been received by Licensor and\n" \
    "      subsequently incorporated within the Work.\n" \
    "\n" \
    "   2. Grant of Copyright License. Subject to the terms and conditions of\n" \
    "      this License, each Contributor hereby grants to You a perpetual,\n" \
    "      worldwide, non-exclusive, no-charge, royalty-free, irrevocable\n" \
    "      copyright license to reproduce, prepare Derivative Works of,\n" \
    "      publicly display, publicly perform, sublicense, and distribute the\n" \
    "      Work and such Derivative Works in Source or Object form.\n" \
    "\n" \
    "   3. Grant of Patent License. Subject to the terms and conditions of\n" \
    "      this License, each Contributor hereby grants to You a perpetual,\n" \
    "      worldwide, non-exclusive, no-charge, royalty-free, irrevocable\n" \
    "      (except as stated in this section) patent license to make, have made,\n" \
    "      use, offer to sell, sell, import, and otherwise transfer the Work,\n" \
    "      where such license applies only to those patent claims licensable\n" \
    "      by such Contributor that are necessarily infringed by their\n" \
    "      Contribution(s) alone or by combination of their Contribution(s)\n" \
    "      with the Work to which such Contribution(s) was submitted. If You\n" \
    "      institute patent litigation against any entity (including a\n" \
    "      cross-claim or counterclaim in a lawsuit) alleging that the Work\n" \
    "      or a Contribution incorporated within the Work constitutes direct\n" \
    "      or contributory patent infringement, then any patent licenses\n" \
    "      granted to You under this License for that Work shall terminate\n" \
    "      as of the date such litigation is filed.\n" \
    "\n" \
    "   4. Redistribution. You may reproduce and distribute copies of the\n" \
    "      Work or Derivative Works thereof in any medium, with or without\n" \
    "      modifications, and in Source or Object form, provided that You\n" \
    "      meet the following conditions:\n" \
    "\n" \
    "      (a) You must give any other recipients of the Work or\n" \
    "          Derivative Works a copy of this License; and\n" \
    "\n" \
    "      (b) You must cause any modified files to carry prominent notices\n" \
    "          stating that You changed the files; and\n" \
    "\n" \
    "      (c) You must retain, in the Source form of any Derivative Works\n" \
    "          that You distribute, all copyright, patent, trademark, and\n" \
    "          attribution notices from the Source form of the Work,\n" \
    "          excluding those notices that do not pertain to any part of\n" \
    "          the Derivative Works; and\n" \
    "\n" \
    "      (d) If the Work includes a \"NOTICE\" text file as part of its\n" \
    "          distribution, then any Derivative Works that You distribute must\n" \
    "          include a readable copy of the attribution notices contained\n" \
    "          within such NOTICE file, excluding those notices that do not\n" \
    "          pertain to any part of the Derivative Works, in at least one\n" \
    "          of the following places: within a NOTICE text file distributed\n" \
    "          as part of the Derivative Works; within the Source form or\n" \
    "          documentation, if provided along with the Derivative Works; or,\n" \
    "          within a display generated by the Derivative Works, if and\n" \
    "          wherever such third-party notices normally appear. The contents\n" \
    "          of the NOTICE file are for informational purposes only and\n" \
    "          do not modify the License. You may add Your own attribution\n" \
    "          notices within Derivative Works that You distribute, alongside\n" \
    "          or as an addendum to the NOTICE text from the Work, provided\n" \
    "          that such additional attribution notices cannot be construed\n" \
    "          as modifying the License.\n" \
    "\n" \
    "      You may add Your own copyright statement to Your modifications and\n" \
    "      may provide additional or different license terms and conditions\n" \
    "      for use, reproduction, or distribution of Your modifications, or\n" \
    "      for any such Derivative Works as a whole, provided Your use,\n" \
    "      reproduction, and distribution of the Work otherwise complies with\n" \
    "      the conditions stated in this License.\n" \
    "\n" \
    "   5. Submission of Contributions. Unless You explicitly state otherwise,\n" \
    "      any Contribution intentionally submitted for inclusion in the Work\n" \
    "      by You to the Licensor shall be under the terms and conditions of\n" \
    "      this License, without any additional terms or conditions.\n" \
    "      Notwithstanding the above, nothing herein shall supersede or modify\n" \
    "      the terms of any separate license agreement you may have executed\n" \
    "      with Licensor regarding such Contributions.\n" \
    "\n" \
    "   6. Trademarks. This License does not grant permission to use the trade\n" \
    "      names, trademarks, service marks, or product names of the Licensor,\n" \
    "      except as required for reasonable and customary use in describing the\n" \
    "      origin of the Work and reproducing the content of the NOTICE file.\n" \
    "\n" \
    "   7. Disclaimer of Warranty. Unless required by applicable law or\n" \
    "      agreed to in writing, Licensor provides the Work (and each\n" \
    "      Contributor provides its Contributions) on an \"AS IS\" BASIS,\n" \
    "      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or\n" \
    "      implied, including, without limitation, any warranties or conditions\n" \
    "      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A\n" \
    "      PARTICULAR PURPOSE. You are solely responsible for determining the\n" \
    "      appropriateness of using or redistributing the Work and assume any\n" \
    "      risks associated with Your exercise of permissions under this License.\n" \
    "\n" \
    "   8. Limitation of Liability. In no event and under no legal theory,\n" \
    "      whether in tort (including negligence), contract, or otherwise,\n" \
    "      unless required by applicable law (such as deliberate and grossly\n" \
    "      negligent acts) or agreed to in writing, shall any Contributor be\n" \
    "      liable to You for damages, including any direct, indirect, special,\n" \
    "      incidental, or consequential damages of any character arising as a\n" \
    "      result of this License or out of the use or inability to use the\n" \
    "      Work (including but not limited to damages for loss of goodwill,\n" \
    "      work stoppage, computer failure or malfunction, or any and all\n" \
    "      other commercial damages or losses), even if such Contributor\n" \
    "      has been advised of the possibility of such damages.\n" \
    "\n" \
    "   9. Accepting Warranty or Additional Liability. While redistributing\n" \
    "      the Work or Derivative Works thereof, You may choose to offer,\n" \
    "      and charge a fee for, acceptance of support, warranty, indemnity,\n" \
    "      or other liability obligations and/or rights consistent with this\n" \
    "      License. However, in accepting such obligations, You may act only\n" \
    "      on Your own behalf and on Your sole responsibility, not on behalf\n" \
    "      of any other Contributor, and only if You agree to indemnify,\n" \
    "      defend, and hold each Contributor harmless for any liability\n" \
    "      incurred by, or claims asserted against, such Contributor by reason\n" \
    "      of your accepting any such warranty or additional liability.\n" \
    "\n" \
    "   END OF TERMS AND CONDITIONS\n" \
    "\n" \
    "==============================================================================\n" \
    "3. SDL3 -- zlib License\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright © 1997-2026 Sam Lantinga <slouken@libsdl.org>\n" \
    "  \n" \
    "This software is provided 'as-is', without any express or implied\n" \
    "warranty.  In no event will the authors be held liable for any damages\n" \
    "arising from the use of this software.\n" \
    "\n" \
    "Permission is granted to anyone to use this software for any purpose,\n" \
    "including commercial applications, and to alter it and redistribute it\n" \
    "freely, subject to the following restrictions:\n" \
    "  \n" \
    "1. The origin of this software must not be misrepresented; you must not\n" \
    "   claim that you wrote the original software. If you use this software\n" \
    "   in a product, an acknowledgment in the product documentation would be\n" \
    "   appreciated but is not required. \n" \
    "2. Altered source versions must be plainly marked as such, and must not be\n" \
    "   misrepresented as being the original software.\n" \
    "3. This notice may not be removed or altered from any source distribution.\n" \
    "\n" \
    "==============================================================================\n" \
    "4. zlib -- zlib License\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright © 1995-2024 Jean-loup Gailly and Mark Adler\n" \
    "\n" \
    "This software is provided 'as-is', without any express or implied\n" \
    "warranty.  In no event will the authors be held liable for any damages\n" \
    "arising from the use of this software.\n" \
    "\n" \
    "Permission is granted to anyone to use this software for any purpose,\n" \
    "including commercial applications, and to alter it and redistribute it\n" \
    "freely, subject to the following restrictions:\n" \
    "\n" \
    "1. The origin of this software must not be misrepresented; you must not\n" \
    "   claim that you wrote the original software. If you use this software\n" \
    "   in a product, an acknowledgment in the product documentation would be\n" \
    "   appreciated but is not required.\n" \
    "2. Altered source versions must be plainly marked as such, and must not be\n" \
    "   misrepresented as being the original software.\n" \
    "3. This notice may not be removed or altered from any source distribution.\n" \
    "\n" \
    "Jean-loup Gailly        Mark Adler\n" \
    "\n" \
    "==============================================================================\n" \
    "5. kjmp2 (MP2 decoder) -- zlib License\n" \
    "==============================================================================\n" \
    "\n" \
    "kjmp2 (third_party/kjmp2), a minimal MPEG-1/2 Audio Layer II decoder by\n" \
    "Martin J. Fiedler. Reproduced from the licence block at the head of\n" \
    "kjmp2.c; the decoder library is under the zlib licence, and the player\n" \
    "applications distributed with it upstream, which PsyMP3 does not use, are\n" \
    "public domain.\n" \
    "\n" \
    "Copyright © 2006-2013 Martin J. Fiedler <martin.fiedler@gmx.net>\n" \
    "\n" \
    "This software is provided 'as-is', without any express or implied\n" \
    "warranty. In no event will the authors be held liable for any damages\n" \
    "arising from the use of this software.\n" \
    "\n" \
    "Permission is granted to anyone to use this software for any purpose,\n" \
    "including commercial applications, and to alter it and redistribute it\n" \
    "freely, subject to the following restrictions:\n" \
    "  1. The origin of this software must not be misrepresented; you must not\n" \
    "     claim that you wrote the original software. If you use this software\n" \
    "     in a product, an acknowledgment in the product documentation would\n" \
    "     be appreciated but is not required.\n" \
    "  2. Altered source versions must be plainly marked as such, and must not\n" \
    "     be misrepresented as being the original software.\n" \
    "  3. This notice may not be removed or altered from any source\n" \
    "     distribution.\n" \
    "\n" \
    "==============================================================================\n" \
    "6. FreeType -- FreeType License (FTL)\n" \
    "==============================================================================\n" \
    "\n" \
    "                    The FreeType Project LICENSE\n" \
    "                    ----------------------------\n" \
    "\n" \
    "                            2006-Jan-27\n" \
    "\n" \
    "                    Copyright 1996-2002, 2006 by\n" \
    "          David Turner, Robert Wilhelm, and Werner Lemberg\n" \
    "\n" \
    "\n" \
    "\n" \
    "Introduction\n" \
    "============\n" \
    "\n" \
    "  The FreeType  Project is distributed in  several archive packages;\n" \
    "  some of them may contain, in addition to the FreeType font engine,\n" \
    "  various tools and  contributions which rely on, or  relate to, the\n" \
    "  FreeType Project.\n" \
    "\n" \
    "  This  license applies  to all  files found  in such  packages, and\n" \
    "  which do not  fall under their own explicit  license.  The license\n" \
    "  affects  thus  the  FreeType   font  engine,  the  test  programs,\n" \
    "  documentation and makefiles, at the very least.\n" \
    "\n" \
    "  This  license   was  inspired  by  the  BSD,   Artistic,  and  IJG\n" \
    "  (Independent JPEG  Group) licenses, which  all encourage inclusion\n" \
    "  and  use of  free  software in  commercial  and freeware  products\n" \
    "  alike.  As a consequence, its main points are that:\n" \
    "\n" \
    "    o We don't promise that this software works. However, we will be\n" \
    "      interested in any kind of bug reports. (`as is' distribution)\n" \
    "\n" \
    "    o You can  use this software for whatever you  want, in parts or\n" \
    "      full form, without having to pay us. (`royalty-free' usage)\n" \
    "\n" \
    "    o You may not pretend that  you wrote this software.  If you use\n" \
    "      it, or  only parts of it,  in a program,  you must acknowledge\n" \
    "      somewhere  in  your  documentation  that  you  have  used  the\n" \
    "      FreeType code. (`credits')\n" \
    "\n" \
    "  We  specifically  permit  and  encourage  the  inclusion  of  this\n" \
    "  software, with  or without modifications,  in commercial products.\n" \
    "  We  disclaim  all warranties  covering  The  FreeType Project  and\n" \
    "  assume no liability related to The FreeType Project.\n" \
    "\n" \
    "\n" \
    "  Finally,  many  people  asked  us  for  a  preferred  form  for  a\n" \
    "  credit/disclaimer to use in compliance with this license.  We thus\n" \
    "  encourage you to use the following text:\n" \
    "\n" \
    "   \"\"\"\n" \
    "    Portions of this software are copyright © <year> The FreeType\n" \
    "    Project (https://freetype.org).  All rights reserved.\n" \
    "   \"\"\"\n" \
    "\n" \
    "  Please replace <year> with the value from the FreeType version you\n" \
    "  actually use.\n" \
    "\n" \
    "\n" \
    "Legal Terms\n" \
    "===========\n" \
    "\n" \
    "0. Definitions\n" \
    "--------------\n" \
    "\n" \
    "  Throughout this license,  the terms `package', `FreeType Project',\n" \
    "  and  `FreeType  archive' refer  to  the  set  of files  originally\n" \
    "  distributed  by the  authors  (David Turner,  Robert Wilhelm,  and\n" \
    "  Werner Lemberg) as the `FreeType Project', be they named as alpha,\n" \
    "  beta or final release.\n" \
    "\n" \
    "  `You' refers to  the licensee, or person using  the project, where\n" \
    "  `using' is a generic term including compiling the project's source\n" \
    "  code as  well as linking it  to form a  `program' or `executable'.\n" \
    "  This  program is  referred to  as  `a program  using the  FreeType\n" \
    "  engine'.\n" \
    "\n" \
    "  This  license applies  to all  files distributed  in  the original\n" \
    "  FreeType  Project,   including  all  source   code,  binaries  and\n" \
    "  documentation,  unless  otherwise  stated   in  the  file  in  its\n" \
    "  original, unmodified form as  distributed in the original archive.\n" \
    "  If you are  unsure whether or not a particular  file is covered by\n" \
    "  this license, you must contact us to verify this.\n" \
    "\n" \
    "  The FreeType  Project is copyright © 1996-2000  by David Turner,\n" \
    "  Robert Wilhelm, and Werner Lemberg.  All rights reserved except as\n" \
    "  specified below.\n" \
    "\n" \
    "1. No Warranty\n" \
    "--------------\n" \
    "\n" \
    "  THE FREETYPE PROJECT  IS PROVIDED `AS IS' WITHOUT  WARRANTY OF ANY\n" \
    "  KIND, EITHER  EXPRESS OR IMPLIED,  INCLUDING, BUT NOT  LIMITED TO,\n" \
    "  WARRANTIES  OF  MERCHANTABILITY   AND  FITNESS  FOR  A  PARTICULAR\n" \
    "  PURPOSE.  IN NO EVENT WILL ANY OF THE AUTHORS OR COPYRIGHT HOLDERS\n" \
    "  BE LIABLE  FOR ANY DAMAGES CAUSED  BY THE USE OR  THE INABILITY TO\n" \
    "  USE, OF THE FREETYPE PROJECT.\n" \
    "\n" \
    "2. Redistribution\n" \
    "-----------------\n" \
    "\n" \
    "  This  license  grants  a  worldwide, royalty-free,  perpetual  and\n" \
    "  irrevocable right  and license to use,  execute, perform, compile,\n" \
    "  display,  copy,   create  derivative  works   of,  distribute  and\n" \
    "  sublicense the  FreeType Project (in  both source and  object code\n" \
    "  forms)  and  derivative works  thereof  for  any  purpose; and  to\n" \
    "  authorize others  to exercise  some or all  of the  rights granted\n" \
    "  herein, subject to the following conditions:\n" \
    "\n" \
    "    o Redistribution of  source code  must retain this  license file\n" \
    "      (`FTL.TXT') unaltered; any  additions, deletions or changes to\n" \
    "      the original  files must be clearly  indicated in accompanying\n" \
    "      documentation.   The  copyright   notices  of  the  unaltered,\n" \
    "      original  files must  be  preserved in  all  copies of  source\n" \
    "      files.\n" \
    "\n" \
    "    o Redistribution in binary form must provide a  disclaimer  that\n" \
    "      states  that  the software is based in part of the work of the\n" \
    "      FreeType Team,  in  the  distribution  documentation.  We also\n" \
    "      encourage you to put an URL to the FreeType web page  in  your\n" \
    "      documentation, though this isn't mandatory.\n" \
    "\n" \
    "  These conditions  apply to any  software derived from or  based on\n" \
    "  the FreeType Project,  not just the unmodified files.   If you use\n" \
    "  our work, you  must acknowledge us.  However, no  fee need be paid\n" \
    "  to us.\n" \
    "\n" \
    "3. Advertising\n" \
    "--------------\n" \
    "\n" \
    "  Neither the  FreeType authors and  contributors nor you  shall use\n" \
    "  the name of the  other for commercial, advertising, or promotional\n" \
    "  purposes without specific prior written permission.\n" \
    "\n" \
    "  We suggest,  but do not require, that  you use one or  more of the\n" \
    "  following phrases to refer  to this software in your documentation\n" \
    "  or advertising  materials: `FreeType Project',  `FreeType Engine',\n" \
    "  `FreeType library', or `FreeType Distribution'.\n" \
    "\n" \
    "  As  you have  not signed  this license,  you are  not  required to\n" \
    "  accept  it.   However,  as  the FreeType  Project  is  copyrighted\n" \
    "  material, only  this license, or  another one contracted  with the\n" \
    "  authors, grants you  the right to use, distribute,  and modify it.\n" \
    "  Therefore,  by  using,  distributing,  or modifying  the  FreeType\n" \
    "  Project, you indicate that you understand and accept all the terms\n" \
    "  of this license.\n" \
    "\n" \
    "4. Contacts\n" \
    "-----------\n" \
    "\n" \
    "  There are two mailing lists related to FreeType:\n" \
    "\n" \
    "    o freetype@nongnu.org\n" \
    "\n" \
    "      Discusses general use and applications of FreeType, as well as\n" \
    "      future and  wanted additions to the  library and distribution.\n" \
    "      If  you are looking  for support,  start in  this list  if you\n" \
    "      haven't found anything to help you in the documentation.\n" \
    "\n" \
    "    o freetype-devel@nongnu.org\n" \
    "\n" \
    "      Discusses bugs,  as well  as engine internals,  design issues,\n" \
    "      specific licenses, porting, etc.\n" \
    "\n" \
    "  Our home page can be found at\n" \
    "\n" \
    "    https://freetype.org\n" \
    "\n" \
    "\n" \
    "--- end of FTL.TXT ---\n" \
    "\n" \
    "==============================================================================\n" \
    "7. HarfBuzz -- \"Old MIT\" License\n" \
    "==============================================================================\n" \
    "\n" \
    "HarfBuzz shapes complex scripts: it chooses the contextual letter\n" \
    "forms Arabic and the Indic scripts require, which a plain\n" \
    "character-to-glyph mapping cannot produce.\n" \
    "\n" \
    "HarfBuzz is licensed under the so-called \"Old MIT\" license.  Details follow.\n" \
    "For parts of HarfBuzz that are licensed under different licenses see individual\n" \
    "files names COPYING in subdirectories where applicable.\n" \
    "\n" \
    "Copyright © 2010-2022  Google, Inc.\n" \
    "Copyright © 2015-2020  Ebrahim Byagowi\n" \
    "Copyright © 2019,2020  Facebook, Inc.\n" \
    "Copyright © 2012,2015  Mozilla Foundation\n" \
    "Copyright © 2011  Codethink Limited\n" \
    "Copyright © 2008,2010  Nokia Corporation and/or its subsidiary(-ies)\n" \
    "Copyright © 2009  Keith Stribley\n" \
    "Copyright © 2011  Martin Hosken and SIL International\n" \
    "Copyright © 2007  Chris Wilson\n" \
    "Copyright © 2005,2006,2020,2021,2022,2023  Behdad Esfahbod\n" \
    "Copyright © 2004,2007,2008,2009,2010,2013,2021,2022,2023  Red Hat, Inc.\n" \
    "Copyright © 1998-2005  David Turner and Werner Lemberg\n" \
    "Copyright © 2016  Igalia S.L.\n" \
    "Copyright © 2022  Matthias Clasen\n" \
    "Copyright © 2018,2021  Khaled Hosny\n" \
    "Copyright © 2018,2019,2020  Adobe, Inc\n" \
    "Copyright © 2013-2015  Alexei Podtelezhnikov\n" \
    "\n" \
    "For full copyright notices consult the individual files in the package.\n" \
    "\n" \
    "\n" \
    "Permission is hereby granted, without written agreement and without\n" \
    "license or royalty fees, to use, copy, modify, and distribute this\n" \
    "software and its documentation for any purpose, provided that the\n" \
    "above copyright notice and the following two paragraphs appear in\n" \
    "all copies of this software.\n" \
    "\n" \
    "IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR\n" \
    "DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES\n" \
    "ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN\n" \
    "IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH\n" \
    "DAMAGE.\n" \
    "\n" \
    "THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,\n" \
    "BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND\n" \
    "FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS\n" \
    "ON AN \"AS IS\" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO\n" \
    "PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.\n" \
    "\n" \
    "==============================================================================\n" \
    "8. TagLib -- Mozilla Public License 1.1\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright © 2002-2026 Scott Wheeler and the TagLib contributors\n" \
    "TagLib is dual-licensed LGPL-2.1 / MPL-1.1; PsyMP3 elects MPL-1.1.\n" \
    "\n" \
    "                          MOZILLA PUBLIC LICENSE\n" \
    "                                Version 1.1\n" \
    "\n" \
    "                              ---------------\n" \
    "\n" \
    "1. Definitions.\n" \
    "\n" \
    "     1.0.1. \"Commercial Use\" means distribution or otherwise making the\n" \
    "     Covered Code available to a third party.\n" \
    "\n" \
    "     1.1. \"Contributor\" means each entity that creates or contributes to\n" \
    "     the creation of Modifications.\n" \
    "\n" \
    "     1.2. \"Contributor Version\" means the combination of the Original\n" \
    "     Code, prior Modifications used by a Contributor, and the Modifications\n" \
    "     made by that particular Contributor.\n" \
    "\n" \
    "     1.3. \"Covered Code\" means the Original Code or Modifications or the\n" \
    "     combination of the Original Code and Modifications, in each case\n" \
    "     including portions thereof.\n" \
    "\n" \
    "     1.4. \"Electronic Distribution Mechanism\" means a mechanism generally\n" \
    "     accepted in the software development community for the electronic\n" \
    "     transfer of data.\n" \
    "\n" \
    "     1.5. \"Executable\" means Covered Code in any form other than Source\n" \
    "     Code.\n" \
    "\n" \
    "     1.6. \"Initial Developer\" means the individual or entity identified\n" \
    "     as the Initial Developer in the Source Code notice required by Exhibit\n" \
    "     A.\n" \
    "\n" \
    "     1.7. \"Larger Work\" means a work which combines Covered Code or\n" \
    "     portions thereof with code not governed by the terms of this License.\n" \
    "\n" \
    "     1.8. \"License\" means this document.\n" \
    "\n" \
    "     1.8.1. \"Licensable\" means having the right to grant, to the maximum\n" \
    "     extent possible, whether at the time of the initial grant or\n" \
    "     subsequently acquired, any and all of the rights conveyed herein.\n" \
    "\n" \
    "     1.9. \"Modifications\" means any addition to or deletion from the\n" \
    "     substance or structure of either the Original Code or any previous\n" \
    "     Modifications. When Covered Code is released as a series of files, a\n" \
    "     Modification is:\n" \
    "          A. Any addition to or deletion from the contents of a file\n" \
    "          containing Original Code or previous Modifications.\n" \
    "\n" \
    "          B. Any new file that contains any part of the Original Code or\n" \
    "          previous Modifications.\n" \
    "\n" \
    "     1.10. \"Original Code\" means Source Code of computer software code\n" \
    "     which is described in the Source Code notice required by Exhibit A as\n" \
    "     Original Code, and which, at the time of its release under this\n" \
    "     License is not already Covered Code governed by this License.\n" \
    "\n" \
    "     1.10.1. \"Patent Claims\" means any patent claim(s), now owned or\n" \
    "     hereafter acquired, including without limitation,  method, process,\n" \
    "     and apparatus claims, in any patent Licensable by grantor.\n" \
    "\n" \
    "     1.11. \"Source Code\" means the preferred form of the Covered Code for\n" \
    "     making modifications to it, including all modules it contains, plus\n" \
    "     any associated interface definition files, scripts used to control\n" \
    "     compilation and installation of an Executable, or source code\n" \
    "     differential comparisons against either the Original Code or another\n" \
    "     well known, available Covered Code of the Contributor's choice. The\n" \
    "     Source Code can be in a compressed or archival form, provided the\n" \
    "     appropriate decompression or de-archiving software is widely available\n" \
    "     for no charge.\n" \
    "\n" \
    "     1.12. \"You\" (or \"Your\")  means an individual or a legal entity\n" \
    "     exercising rights under, and complying with all of the terms of, this\n" \
    "     License or a future version of this License issued under Section 6.1.\n" \
    "     For legal entities, \"You\" includes any entity which controls, is\n" \
    "     controlled by, or is under common control with You. For purposes of\n" \
    "     this definition, \"control\" means (a) the power, direct or indirect,\n" \
    "     to cause the direction or management of such entity, whether by\n" \
    "     contract or otherwise, or (b) ownership of more than fifty percent\n" \
    "     (50%) of the outstanding shares or beneficial ownership of such\n" \
    "     entity.\n" \
    "\n" \
    "2. Source Code License.\n" \
    "\n" \
    "     2.1. The Initial Developer Grant.\n" \
    "     The Initial Developer hereby grants You a world-wide, royalty-free,\n" \
    "     non-exclusive license, subject to third party intellectual property\n" \
    "     claims:\n" \
    "          (a)  under intellectual property rights (other than patent or\n" \
    "          trademark) Licensable by Initial Developer to use, reproduce,\n" \
    "          modify, display, perform, sublicense and distribute the Original\n" \
    "          Code (or portions thereof) with or without Modifications, and/or\n" \
    "          as part of a Larger Work; and\n" \
    "\n" \
    "          (b) under Patents Claims infringed by the making, using or\n" \
    "          selling of Original Code, to make, have made, use, practice,\n" \
    "          sell, and offer for sale, and/or otherwise dispose of the\n" \
    "          Original Code (or portions thereof).\n" \
    "\n" \
    "          (c) the licenses granted in this Section 2.1(a) and (b) are\n" \
    "          effective on the date Initial Developer first distributes\n" \
    "          Original Code under the terms of this License.\n" \
    "\n" \
    "          (d) Notwithstanding Section 2.1(b) above, no patent license is\n" \
    "          granted: 1) for code that You delete from the Original Code; 2)\n" \
    "          separate from the Original Code;  or 3) for infringements caused\n" \
    "          by: i) the modification of the Original Code or ii) the\n" \
    "          combination of the Original Code with other software or devices.\n" \
    "\n" \
    "     2.2. Contributor Grant.\n" \
    "     Subject to third party intellectual property claims, each Contributor\n" \
    "     hereby grants You a world-wide, royalty-free, non-exclusive license\n" \
    "\n" \
    "          (a)  under intellectual property rights (other than patent or\n" \
    "          trademark) Licensable by Contributor, to use, reproduce, modify,\n" \
    "          display, perform, sublicense and distribute the Modifications\n" \
    "          created by such Contributor (or portions thereof) either on an\n" \
    "          unmodified basis, with other Modifications, as Covered Code\n" \
    "          and/or as part of a Larger Work; and\n" \
    "\n" \
    "          (b) under Patent Claims infringed by the making, using, or\n" \
    "          selling of  Modifications made by that Contributor either alone\n" \
    "          and/or in combination with its Contributor Version (or portions\n" \
    "          of such combination), to make, use, sell, offer for sale, have\n" \
    "          made, and/or otherwise dispose of: 1) Modifications made by that\n" \
    "          Contributor (or portions thereof); and 2) the combination of\n" \
    "          Modifications made by that Contributor with its Contributor\n" \
    "          Version (or portions of such combination).\n" \
    "\n" \
    "          (c) the licenses granted in Sections 2.2(a) and 2.2(b) are\n" \
    "          effective on the date Contributor first makes Commercial Use of\n" \
    "          the Covered Code.\n" \
    "\n" \
    "          (d)    Notwithstanding Section 2.2(b) above, no patent license is\n" \
    "          granted: 1) for any code that Contributor has deleted from the\n" \
    "          Contributor Version; 2)  separate from the Contributor Version;\n" \
    "          3)  for infringements caused by: i) third party modifications of\n" \
    "          Contributor Version or ii)  the combination of Modifications made\n" \
    "          by that Contributor with other software  (except as part of the\n" \
    "          Contributor Version) or other devices; or 4) under Patent Claims\n" \
    "          infringed by Covered Code in the absence of Modifications made by\n" \
    "          that Contributor.\n" \
    "\n" \
    "3. Distribution Obligations.\n" \
    "\n" \
    "     3.1. Application of License.\n" \
    "     The Modifications which You create or to which You contribute are\n" \
    "     governed by the terms of this License, including without limitation\n" \
    "     Section 2.2. The Source Code version of Covered Code may be\n" \
    "     distributed only under the terms of this License or a future version\n" \
    "     of this License released under Section 6.1, and You must include a\n" \
    "     copy of this License with every copy of the Source Code You\n" \
    "     distribute. You may not offer or impose any terms on any Source Code\n" \
    "     version that alters or restricts the applicable version of this\n" \
    "     License or the recipients' rights hereunder. However, You may include\n" \
    "     an additional document offering the additional rights described in\n" \
    "     Section 3.5.\n" \
    "\n" \
    "     3.2. Availability of Source Code.\n" \
    "     Any Modification which You create or to which You contribute must be\n" \
    "     made available in Source Code form under the terms of this License\n" \
    "     either on the same media as an Executable version or via an accepted\n" \
    "     Electronic Distribution Mechanism to anyone to whom you made an\n" \
    "     Executable version available; and if made available via Electronic\n" \
    "     Distribution Mechanism, must remain available for at least twelve (12)\n" \
    "     months after the date it initially became available, or at least six\n" \
    "     (6) months after a subsequent version of that particular Modification\n" \
    "     has been made available to such recipients. You are responsible for\n" \
    "     ensuring that the Source Code version remains available even if the\n" \
    "     Electronic Distribution Mechanism is maintained by a third party.\n" \
    "\n" \
    "     3.3. Description of Modifications.\n" \
    "     You must cause all Covered Code to which You contribute to contain a\n" \
    "     file documenting the changes You made to create that Covered Code and\n" \
    "     the date of any change. You must include a prominent statement that\n" \
    "     the Modification is derived, directly or indirectly, from Original\n" \
    "     Code provided by the Initial Developer and including the name of the\n" \
    "     Initial Developer in (a) the Source Code, and (b) in any notice in an\n" \
    "     Executable version or related documentation in which You describe the\n" \
    "     origin or ownership of the Covered Code.\n" \
    "\n" \
    "     3.4. Intellectual Property Matters\n" \
    "          (a) Third Party Claims.\n" \
    "          If Contributor has knowledge that a license under a third party's\n" \
    "          intellectual property rights is required to exercise the rights\n" \
    "          granted by such Contributor under Sections 2.1 or 2.2,\n" \
    "          Contributor must include a text file with the Source Code\n" \
    "          distribution titled \"LEGAL\" which describes the claim and the\n" \
    "          party making the claim in sufficient detail that a recipient will\n" \
    "          know whom to contact. If Contributor obtains such knowledge after\n" \
    "          the Modification is made available as described in Section 3.2,\n" \
    "          Contributor shall promptly modify the LEGAL file in all copies\n" \
    "          Contributor makes available thereafter and shall take other steps\n" \
    "          (such as notifying appropriate mailing lists or newsgroups)\n" \
    "          reasonably calculated to inform those who received the Covered\n" \
    "          Code that new knowledge has been obtained.\n" \
    "\n" \
    "          (b) Contributor APIs.\n" \
    "          If Contributor's Modifications include an application programming\n" \
    "          interface and Contributor has knowledge of patent licenses which\n" \
    "          are reasonably necessary to implement that API, Contributor must\n" \
    "          also include this information in the LEGAL file.\n" \
    "\n" \
    "               (c)    Representations.\n" \
    "          Contributor represents that, except as disclosed pursuant to\n" \
    "          Section 3.4(a) above, Contributor believes that Contributor's\n" \
    "          Modifications are Contributor's original creation(s) and/or\n" \
    "          Contributor has sufficient rights to grant the rights conveyed by\n" \
    "          this License.\n" \
    "\n" \
    "     3.5. Required Notices.\n" \
    "     You must duplicate the notice in Exhibit A in each file of the Source\n" \
    "     Code.  If it is not possible to put such notice in a particular Source\n" \
    "     Code file due to its structure, then You must include such notice in a\n" \
    "     location (such as a relevant directory) where a user would be likely\n" \
    "     to look for such a notice.  If You created one or more Modification(s)\n" \
    "     You may add your name as a Contributor to the notice described in\n" \
    "     Exhibit A.  You must also duplicate this License in any documentation\n" \
    "     for the Source Code where You describe recipients' rights or ownership\n" \
    "     rights relating to Covered Code.  You may choose to offer, and to\n" \
    "     charge a fee for, warranty, support, indemnity or liability\n" \
    "     obligations to one or more recipients of Covered Code. However, You\n" \
    "     may do so only on Your own behalf, and not on behalf of the Initial\n" \
    "     Developer or any Contributor. You must make it absolutely clear than\n" \
    "     any such warranty, support, indemnity or liability obligation is\n" \
    "     offered by You alone, and You hereby agree to indemnify the Initial\n" \
    "     Developer and every Contributor for any liability incurred by the\n" \
    "     Initial Developer or such Contributor as a result of warranty,\n" \
    "     support, indemnity or liability terms You offer.\n" \
    "\n" \
    "     3.6. Distribution of Executable Versions.\n" \
    "     You may distribute Covered Code in Executable form only if the\n" \
    "     requirements of Section 3.1-3.5 have been met for that Covered Code,\n" \
    "     and if You include a notice stating that the Source Code version of\n" \
    "     the Covered Code is available under the terms of this License,\n" \
    "     including a description of how and where You have fulfilled the\n" \
    "     obligations of Section 3.2. The notice must be conspicuously included\n" \
    "     in any notice in an Executable version, related documentation or\n" \
    "     collateral in which You describe recipients' rights relating to the\n" \
    "     Covered Code. You may distribute the Executable version of Covered\n" \
    "     Code or ownership rights under a license of Your choice, which may\n" \
    "     contain terms different from this License, provided that You are in\n" \
    "     compliance with the terms of this License and that the license for the\n" \
    "     Executable version does not attempt to limit or alter the recipient's\n" \
    "     rights in the Source Code version from the rights set forth in this\n" \
    "     License. If You distribute the Executable version under a different\n" \
    "     license You must make it absolutely clear that any terms which differ\n" \
    "     from this License are offered by You alone, not by the Initial\n" \
    "     Developer or any Contributor. You hereby agree to indemnify the\n" \
    "     Initial Developer and every Contributor for any liability incurred by\n" \
    "     the Initial Developer or such Contributor as a result of any such\n" \
    "     terms You offer.\n" \
    "\n" \
    "     3.7. Larger Works.\n" \
    "     You may create a Larger Work by combining Covered Code with other code\n" \
    "     not governed by the terms of this License and distribute the Larger\n" \
    "     Work as a single product. In such a case, You must make sure the\n" \
    "     requirements of this License are fulfilled for the Covered Code.\n" \
    "\n" \
    "4. Inability to Comply Due to Statute or Regulation.\n" \
    "\n" \
    "     If it is impossible for You to comply with any of the terms of this\n" \
    "     License with respect to some or all of the Covered Code due to\n" \
    "     statute, judicial order, or regulation then You must: (a) comply with\n" \
    "     the terms of this License to the maximum extent possible; and (b)\n" \
    "     describe the limitations and the code they affect. Such description\n" \
    "     must be included in the LEGAL file described in Section 3.4 and must\n" \
    "     be included with all distributions of the Source Code. Except to the\n" \
    "     extent prohibited by statute or regulation, such description must be\n" \
    "     sufficiently detailed for a recipient of ordinary skill to be able to\n" \
    "     understand it.\n" \
    "\n" \
    "5. Application of this License.\n" \
    "\n" \
    "     This License applies to code to which the Initial Developer has\n" \
    "     attached the notice in Exhibit A and to related Covered Code.\n" \
    "\n" \
    "6. Versions of the License.\n" \
    "\n" \
    "     6.1. New Versions.\n" \
    "     Netscape Communications Corporation (\"Netscape\") may publish revised\n" \
    "     and/or new versions of the License from time to time. Each version\n" \
    "     will be given a distinguishing version number.\n" \
    "\n" \
    "     6.2. Effect of New Versions.\n" \
    "     Once Covered Code has been published under a particular version of the\n" \
    "     License, You may always continue to use it under the terms of that\n" \
    "     version. You may also choose to use such Covered Code under the terms\n" \
    "     of any subsequent version of the License published by Netscape. No one\n" \
    "     other than Netscape has the right to modify the terms applicable to\n" \
    "     Covered Code created under this License.\n" \
    "\n" \
    "     6.3. Derivative Works.\n" \
    "     If You create or use a modified version of this License (which you may\n" \
    "     only do in order to apply it to code which is not already Covered Code\n" \
    "     governed by this License), You must (a) rename Your license so that\n" \
    "     the phrases \"Mozilla\", \"MOZILLAPL\", \"MOZPL\", \"Netscape\",\n" \
    "     \"MPL\", \"NPL\" or any confusingly similar phrase do not appear in your\n" \
    "     license (except to note that your license differs from this License)\n" \
    "     and (b) otherwise make it clear that Your version of the license\n" \
    "     contains terms which differ from the Mozilla Public License and\n" \
    "     Netscape Public License. (Filling in the name of the Initial\n" \
    "     Developer, Original Code or Contributor in the notice described in\n" \
    "     Exhibit A shall not of themselves be deemed to be modifications of\n" \
    "     this License.)\n" \
    "\n" \
    "7. DISCLAIMER OF WARRANTY.\n" \
    "\n" \
    "     COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN \"AS IS\" BASIS,\n" \
    "     WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,\n" \
    "     WITHOUT LIMITATION, WARRANTIES THAT THE COVERED CODE IS FREE OF\n" \
    "     DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE OR NON-INFRINGING.\n" \
    "     THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED CODE\n" \
    "     IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT,\n" \
    "     YOU (NOT THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE\n" \
    "     COST OF ANY NECESSARY SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER\n" \
    "     OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS LICENSE. NO USE OF\n" \
    "     ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER THIS DISCLAIMER.\n" \
    "\n" \
    "8. TERMINATION.\n" \
    "\n" \
    "     8.1.  This License and the rights granted hereunder will terminate\n" \
    "     automatically if You fail to comply with terms herein and fail to cure\n" \
    "     such breach within 30 days of becoming aware of the breach. All\n" \
    "     sublicenses to the Covered Code which are properly granted shall\n" \
    "     survive any termination of this License. Provisions which, by their\n" \
    "     nature, must remain in effect beyond the termination of this License\n" \
    "     shall survive.\n" \
    "\n" \
    "     8.2.  If You initiate litigation by asserting a patent infringement\n" \
    "     claim (excluding declatory judgment actions) against Initial Developer\n" \
    "     or a Contributor (the Initial Developer or Contributor against whom\n" \
    "     You file such action is referred to as \"Participant\")  alleging that:\n" \
    "\n" \
    "     (a)  such Participant's Contributor Version directly or indirectly\n" \
    "     infringes any patent, then any and all rights granted by such\n" \
    "     Participant to You under Sections 2.1 and/or 2.2 of this License\n" \
    "     shall, upon 60 days notice from Participant terminate prospectively,\n" \
    "     unless if within 60 days after receipt of notice You either: (i)\n" \
    "     agree in writing to pay Participant a mutually agreeable reasonable\n" \
    "     royalty for Your past and future use of Modifications made by such\n" \
    "     Participant, or (ii) withdraw Your litigation claim with respect to\n" \
    "     the Contributor Version against such Participant.  If within 60 days\n" \
    "     of notice, a reasonable royalty and payment arrangement are not\n" \
    "     mutually agreed upon in writing by the parties or the litigation claim\n" \
    "     is not withdrawn, the rights granted by Participant to You under\n" \
    "     Sections 2.1 and/or 2.2 automatically terminate at the expiration of\n" \
    "     the 60 day notice period specified above.\n" \
    "\n" \
    "     (b)  any software, hardware, or device, other than such Participant's\n" \
    "     Contributor Version, directly or indirectly infringes any patent, then\n" \
    "     any rights granted to You by such Participant under Sections 2.1(b)\n" \
    "     and 2.2(b) are revoked effective as of the date You first made, used,\n" \
    "     sold, distributed, or had made, Modifications made by that\n" \
    "     Participant.\n" \
    "\n" \
    "     8.3.  If You assert a patent infringement claim against Participant\n" \
    "     alleging that such Participant's Contributor Version directly or\n" \
    "     indirectly infringes any patent where such claim is resolved (such as\n" \
    "     by license or settlement) prior to the initiation of patent\n" \
    "     infringement litigation, then the reasonable value of the licenses\n" \
    "     granted by such Participant under Sections 2.1 or 2.2 shall be taken\n" \
    "     into account in determining the amount or value of any payment or\n" \
    "     license.\n" \
    "\n" \
    "     8.4.  In the event of termination under Sections 8.1 or 8.2 above,\n" \
    "     all end user license agreements (excluding distributors and resellers)\n" \
    "     which have been validly granted by You or any distributor hereunder\n" \
    "     prior to termination shall survive termination.\n" \
    "\n" \
    "9. LIMITATION OF LIABILITY.\n" \
    "\n" \
    "     UNDER NO CIRCUMSTANCES AND UNDER NO LEGAL THEORY, WHETHER TORT\n" \
    "     (INCLUDING NEGLIGENCE), CONTRACT, OR OTHERWISE, SHALL YOU, THE INITIAL\n" \
    "     DEVELOPER, ANY OTHER CONTRIBUTOR, OR ANY DISTRIBUTOR OF COVERED CODE,\n" \
    "     OR ANY SUPPLIER OF ANY OF SUCH PARTIES, BE LIABLE TO ANY PERSON FOR\n" \
    "     ANY INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES OF ANY\n" \
    "     CHARACTER INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF GOODWILL,\n" \
    "     WORK STOPPAGE, COMPUTER FAILURE OR MALFUNCTION, OR ANY AND ALL OTHER\n" \
    "     COMMERCIAL DAMAGES OR LOSSES, EVEN IF SUCH PARTY SHALL HAVE BEEN\n" \
    "     INFORMED OF THE POSSIBILITY OF SUCH DAMAGES. THIS LIMITATION OF\n" \
    "     LIABILITY SHALL NOT APPLY TO LIABILITY FOR DEATH OR PERSONAL INJURY\n" \
    "     RESULTING FROM SUCH PARTY'S NEGLIGENCE TO THE EXTENT APPLICABLE LAW\n" \
    "     PROHIBITS SUCH LIMITATION. SOME JURISDICTIONS DO NOT ALLOW THE\n" \
    "     EXCLUSION OR LIMITATION OF INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO\n" \
    "     THIS EXCLUSION AND LIMITATION MAY NOT APPLY TO YOU.\n" \
    "\n" \
    "10. U.S. GOVERNMENT END USERS.\n" \
    "\n" \
    "     The Covered Code is a \"commercial item,\" as that term is defined in\n" \
    "     48 C.F.R. 2.101 (Oct. 1995), consisting of \"commercial computer\n" \
    "     software\" and \"commercial computer software documentation,\" as such\n" \
    "     terms are used in 48 C.F.R. 12.212 (Sept. 1995). Consistent with 48\n" \
    "     C.F.R. 12.212 and 48 C.F.R. 227.7202-1 through 227.7202-4 (June 1995),\n" \
    "     all U.S. Government End Users acquire Covered Code with only those\n" \
    "     rights set forth herein.\n" \
    "\n" \
    "11. MISCELLANEOUS.\n" \
    "\n" \
    "     This License represents the complete agreement concerning subject\n" \
    "     matter hereof. If any provision of this License is held to be\n" \
    "     unenforceable, such provision shall be reformed only to the extent\n" \
    "     necessary to make it enforceable. This License shall be governed by\n" \
    "     California law provisions (except to the extent applicable law, if\n" \
    "     any, provides otherwise), excluding its conflict-of-law provisions.\n" \
    "     With respect to disputes in which at least one party is a citizen of,\n" \
    "     or an entity chartered or registered to do business in the United\n" \
    "     States of America, any litigation relating to this License shall be\n" \
    "     subject to the jurisdiction of the Federal Courts of the Northern\n" \
    "     District of California, with venue lying in Santa Clara County,\n" \
    "     California, with the losing party responsible for costs, including\n" \
    "     without limitation, court costs and reasonable attorneys' fees and\n" \
    "     expenses. The application of the United Nations Convention on\n" \
    "     Contracts for the International Sale of Goods is expressly excluded.\n" \
    "     Any law or regulation which provides that the language of a contract\n" \
    "     shall be construed against the drafter shall not apply to this\n" \
    "     License.\n" \
    "\n" \
    "12. RESPONSIBILITY FOR CLAIMS.\n" \
    "\n" \
    "     As between Initial Developer and the Contributors, each party is\n" \
    "     responsible for claims and damages arising, directly or indirectly,\n" \
    "     out of its utilization of rights under this License and You agree to\n" \
    "     work with Initial Developer and Contributors to distribute such\n" \
    "     responsibility on an equitable basis. Nothing herein is intended or\n" \
    "     shall be deemed to constitute any admission of liability.\n" \
    "\n" \
    "13. MULTIPLE-LICENSED CODE.\n" \
    "\n" \
    "     Initial Developer may designate portions of the Covered Code as\n" \
    "     \"Multiple-Licensed\".  \"Multiple-Licensed\" means that the Initial\n" \
    "     Developer permits you to utilize portions of the Covered Code under\n" \
    "     Your choice of the MPL or the alternative licenses, if any, specified\n" \
    "     by the Initial Developer in the file described in Exhibit A.\n" \
    "\n" \
    "EXHIBIT A -Mozilla Public License.\n" \
    "\n" \
    "     ``The contents of this file are subject to the Mozilla Public License\n" \
    "     Version 1.1 (the \"License\"); you may not use this file except in\n" \
    "     compliance with the License. You may obtain a copy of the License at\n" \
    "     https://www.mozilla.org/MPL/\n" \
    "\n" \
    "     Software distributed under the License is distributed on an \"AS IS\"\n" \
    "     basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the\n" \
    "     License for the specific language governing rights and limitations\n" \
    "     under the License.\n" \
    "\n" \
    "     The Original Code is ______________________________________.\n" \
    "\n" \
    "     The Initial Developer of the Original Code is ________________________.\n" \
    "     Portions created by ______________________ are Copyright © ______\n" \
    "     _______________________. All Rights Reserved.\n" \
    "\n" \
    "     Contributor(s): ______________________________________.\n" \
    "\n" \
    "     Alternatively, the contents of this file may be used under the terms\n" \
    "     of the _____ license (the  \"[___] License\"), in which case the\n" \
    "     provisions of [______] License are applicable instead of those\n" \
    "     above.  If you wish to allow use of your version of this file only\n" \
    "     under the terms of the [____] License and not to allow others to use\n" \
    "     your version of this file under the MPL, indicate your decision by\n" \
    "     deleting  the provisions above and replace  them with the notice and\n" \
    "     other provisions required by the [___] License.  If you do not delete\n" \
    "     the provisions above, a recipient may use your version of this file\n" \
    "     under either the MPL or the [___] License.\"\n" \
    "\n" \
    "     [NOTE: The text of this Exhibit A may differ slightly from the text of\n" \
    "     the notices in the Source Code files of the Original Code. You should\n" \
    "     use the text of this Exhibit A rather than the text found in the\n" \
    "     Original Code Source Code for Your Modifications.]\n" \
    "\n" \
    "==============================================================================\n" \
    "9. FDK-AAC -- Fraunhofer FDK AAC Codec Library for Android license\n" \
    "==============================================================================\n" \
    "\n" \
    "-----------------------------------------------------------------------------\n" \
    "Software License for The Fraunhofer FDK AAC Codec Library for Android\n" \
    "\n" \
    "© Copyright  1995 - 2019 Fraunhofer-Gesellschaft zur Förderung der angewandten\n" \
    "Forschung e.V. All rights reserved.\n" \
    "\n" \
    " 1.    INTRODUCTION\n" \
    "The Fraunhofer FDK AAC Codec Library for Android (\"FDK AAC Codec\") is software\n" \
    "that implements the MPEG Advanced Audio Coding (\"AAC\") encoding and decoding\n" \
    "scheme for digital audio. This FDK AAC Codec software is intended to be used on\n" \
    "a wide variety of Android devices.\n" \
    "\n" \
    "AAC's HE-AAC and HE-AAC v2 versions are regarded as today's most efficient\n" \
    "general perceptual audio codecs. AAC-ELD is considered the best-performing\n" \
    "full-bandwidth communications codec by independent studies and is widely\n" \
    "deployed. AAC has been standardized by ISO and IEC as part of the MPEG\n" \
    "specifications.\n" \
    "\n" \
    "Patent licenses for necessary patent claims for the FDK AAC Codec (including\n" \
    "those of Fraunhofer) may be obtained through Via Licensing\n" \
    "(www.vialicensing.com) or through the respective patent owners individually for\n" \
    "the purpose of encoding or decoding bit streams in products that are compliant\n" \
    "with the ISO/IEC MPEG audio standards. Please note that most manufacturers of\n" \
    "Android devices already license these patent claims through Via Licensing or\n" \
    "directly from the patent owners, and therefore FDK AAC Codec software may\n" \
    "already be covered under those patent licenses when it is used for those\n" \
    "licensed purposes only.\n" \
    "\n" \
    "Commercially-licensed AAC software libraries, including floating-point versions\n" \
    "with enhanced sound quality, are also available from Fraunhofer. Users are\n" \
    "encouraged to check the Fraunhofer website for additional applications\n" \
    "information and documentation.\n" \
    "\n" \
    "2.    COPYRIGHT LICENSE\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without modification,\n" \
    "are permitted without payment of copyright license fees provided that you\n" \
    "satisfy the following conditions:\n" \
    "\n" \
    "You must retain the complete text of this software license in redistributions of\n" \
    "the FDK AAC Codec or your modifications thereto in source code form.\n" \
    "\n" \
    "You must retain the complete text of this software license in the documentation\n" \
    "and/or other materials provided with redistributions of the FDK AAC Codec or\n" \
    "your modifications thereto in binary form. You must make available free of\n" \
    "charge copies of the complete source code of the FDK AAC Codec and your\n" \
    "modifications thereto to recipients of copies in binary form.\n" \
    "\n" \
    "The name of Fraunhofer may not be used to endorse or promote products derived\n" \
    "from this library without prior written permission.\n" \
    "\n" \
    "You may not charge copyright license fees for anyone to use, copy or distribute\n" \
    "the FDK AAC Codec software or your modifications thereto.\n" \
    "\n" \
    "Your modified versions of the FDK AAC Codec must carry prominent notices stating\n" \
    "that you changed the software and the date of any change. For modified versions\n" \
    "of the FDK AAC Codec, the term \"Fraunhofer FDK AAC Codec Library for Android\"\n" \
    "must be replaced by the term \"Third-Party Modified Version of the Fraunhofer FDK\n" \
    "AAC Codec Library for Android.\"\n" \
    "\n" \
    "3.    NO PATENT LICENSE\n" \
    "\n" \
    "NO EXPRESS OR IMPLIED LICENSES TO ANY PATENT CLAIMS, including without\n" \
    "limitation the patents of Fraunhofer, ARE GRANTED BY THIS SOFTWARE LICENSE.\n" \
    "Fraunhofer provides no warranty of patent non-infringement with respect to this\n" \
    "software.\n" \
    "\n" \
    "You may use this FDK AAC Codec software or modifications thereto only for\n" \
    "purposes that are authorized by appropriate patent licenses.\n" \
    "\n" \
    "4.    DISCLAIMER\n" \
    "\n" \
    "This FDK AAC Codec software is provided by Fraunhofer on behalf of the copyright\n" \
    "holders and contributors \"AS IS\" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,\n" \
    "including but not limited to the implied warranties of merchantability and\n" \
    "fitness for a particular purpose. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR\n" \
    "CONTRIBUTORS BE LIABLE for any direct, indirect, incidental, special, exemplary,\n" \
    "or consequential damages, including but not limited to procurement of substitute\n" \
    "goods or services; loss of use, data, or profits, or business interruption,\n" \
    "however caused and on any theory of liability, whether in contract, strict\n" \
    "liability, or tort (including negligence), arising in any way out of the use of\n" \
    "this software, even if advised of the possibility of such damage.\n" \
    "\n" \
    "5.    CONTACT INFORMATION\n" \
    "\n" \
    "Fraunhofer Institute for Integrated Circuits IIS\n" \
    "Attention: Audio and Multimedia Departments - FDK AAC LL\n" \
    "Am Wolfsmantel 33\n" \
    "91058 Erlangen, Germany\n" \
    "\n" \
    "www.iis.fraunhofer.de/amm\n" \
    "amm-info@iis.fraunhofer.de\n" \
    "-----------------------------------------------------------------------------\n" \
    "\n" \
    "==============================================================================\n" \
    "10. libogg -- BSD 3-Clause\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright © 2002, Xiph.org Foundation\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without\n" \
    "modification, are permitted provided that the following conditions\n" \
    "are met:\n" \
    "\n" \
    "- Redistributions of source code must retain the above copyright\n" \
    "notice, this list of conditions and the following disclaimer.\n" \
    "\n" \
    "- Redistributions in binary form must reproduce the above copyright\n" \
    "notice, this list of conditions and the following disclaimer in the\n" \
    "documentation and/or other materials provided with the distribution.\n" \
    "\n" \
    "- Neither the name of the Xiph.org Foundation nor the names of its\n" \
    "contributors may be used to endorse or promote products derived from\n" \
    "this software without specific prior written permission.\n" \
    "\n" \
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n" \
    "``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n" \
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n" \
    "A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION\n" \
    "OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n" \
    "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n" \
    "LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n" \
    "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n" \
    "THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n" \
    "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n" \
    "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n" \
    "\n" \
    "==============================================================================\n" \
    "11. Opus -- BSD 3-Clause\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright 2001-2023 Xiph.Org, Skype Limited, Octasic,\n" \
    "                    Jean-Marc Valin, Timothy B. Terriberry,\n" \
    "                    CSIRO, Gregory Maxwell, Mark Borgerding,\n" \
    "                    Erik de Castro Lopo, Mozilla, Amazon\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without\n" \
    "modification, are permitted provided that the following conditions\n" \
    "are met:\n" \
    "\n" \
    "- Redistributions of source code must retain the above copyright\n" \
    "notice, this list of conditions and the following disclaimer.\n" \
    "\n" \
    "- Redistributions in binary form must reproduce the above copyright\n" \
    "notice, this list of conditions and the following disclaimer in the\n" \
    "documentation and/or other materials provided with the distribution.\n" \
    "\n" \
    "- Neither the name of Internet Society, IETF or IETF Trust, nor the\n" \
    "names of specific contributors, may be used to endorse or promote\n" \
    "products derived from this software without specific prior written\n" \
    "permission.\n" \
    "\n" \
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n" \
    "``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n" \
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n" \
    "A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER\n" \
    "OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n" \
    "EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n" \
    "PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n" \
    "PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF\n" \
    "LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING\n" \
    "NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n" \
    "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n" \
    "\n" \
    "Opus is subject to the royalty-free patent licenses which are\n" \
    "specified at:\n" \
    "\n" \
    "Xiph.Org Foundation:\n" \
    "https://datatracker.ietf.org/ipr/1524/\n" \
    "\n" \
    "Microsoft Corporation:\n" \
    "https://datatracker.ietf.org/ipr/1914/\n" \
    "\n" \
    "Broadcom Corporation:\n" \
    "https://datatracker.ietf.org/ipr/1526/\n" \
    "\n" \
    "==============================================================================\n" \
    "12. Speex -- BSD 3-Clause\n" \
    "==============================================================================\n" \
    "\n" \
    "Copyright 2002-2008 	Xiph.org Foundation\n" \
    "Copyright 2002-2008 	Jean-Marc Valin\n" \
    "Copyright 2005-2007	Analog Devices Inc.\n" \
    "Copyright 2005-2008	Commonwealth Scientific and Industrial Research\n" \
    "                        Organisation (CSIRO)\n" \
    "Copyright 1993, 2002, 2006 David Rowe\n" \
    "Copyright 2003 		EpicGames\n" \
    "Copyright 1992-1994	Jutta Degener, Carsten Bormann\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without\n" \
    "modification, are permitted provided that the following conditions\n" \
    "are met:\n" \
    "\n" \
    "- Redistributions of source code must retain the above copyright\n" \
    "notice, this list of conditions and the following disclaimer.\n" \
    "\n" \
    "- Redistributions in binary form must reproduce the above copyright\n" \
    "notice, this list of conditions and the following disclaimer in the\n" \
    "documentation and/or other materials provided with the distribution.\n" \
    "\n" \
    "- Neither the name of the Xiph.org Foundation nor the names of its\n" \
    "contributors may be used to endorse or promote products derived from\n" \
    "this software without specific prior written permission.\n" \
    "\n" \
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n" \
    "``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n" \
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n" \
    "A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR\n" \
    "CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,\n" \
    "EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n" \
    "PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n" \
    "PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF\n" \
    "LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING\n" \
    "NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n" \
    "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n" \
    "\n" \
    "==============================================================================\n" \
    "13. libcurl -- curl License\n" \
    "==============================================================================\n" \
    "\n" \
    "COPYRIGHT AND PERMISSION NOTICE\n" \
    "\n" \
    "Copyright © 1996 - 2026, Daniel Stenberg, <daniel@haxx.se>, and many\n" \
    "contributors, see the THANKS file.\n" \
    "\n" \
    "All rights reserved.\n" \
    "\n" \
    "Permission to use, copy, modify, and distribute this software for any purpose\n" \
    "with or without fee is hereby granted, provided that the above copyright\n" \
    "notice and this permission notice appear in all copies.\n" \
    "\n" \
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n" \
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n" \
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN\n" \
    "NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,\n" \
    "DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR\n" \
    "OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE\n" \
    "OR OTHER DEALINGS IN THE SOFTWARE.\n" \
    "\n" \
    "Except as contained in this notice, the name of a copyright holder shall not\n" \
    "be used in advertising or otherwise to promote the sale, use or other dealings\n" \
    "in this Software without prior written authorization of the copyright holder.\n" \
    "\n" \
    "==============================================================================\n" \
    "14. pugixml -- MIT License\n" \
    "==============================================================================\n" \
    "\n" \
    "MIT License\n" \
    "\n" \
    "Copyright © 2006-2026 Arseny Kapoulkine\n" \
    "\n" \
    "Permission is hereby granted, free of charge, to any person\n" \
    "obtaining a copy of this software and associated documentation\n" \
    "files (the \"Software\"), to deal in the Software without\n" \
    "restriction, including without limitation the rights to use,\n" \
    "copy, modify, merge, publish, distribute, sublicense, and/or sell\n" \
    "copies of the Software, and to permit persons to whom the\n" \
    "Software is furnished to do so, subject to the following\n" \
    "conditions:\n" \
    "\n" \
    "The above copyright notice and this permission notice shall be\n" \
    "included in all copies or substantial portions of the Software.\n" \
    "\n" \
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n" \
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES\n" \
    "OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n" \
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT\n" \
    "HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n" \
    "WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n" \
    "FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR\n" \
    "OTHER DEALINGS IN THE SOFTWARE.\n" \
    "\n" \
    "==============================================================================\n" \
    "15. stb_vorbis -- Public Domain (Unlicense)\n" \
    "==============================================================================\n" \
    "\n" \
    "stb_vorbis (third_party/stb/stb_vorbis.c), written by Sean Barrett, is\n" \
    "offered under a choice of the MIT License or a public domain dedication.\n" \
    "PsyMP3 elects the public domain dedication, reproduced here.\n" \
    "\n" \
    "Public Domain (www.unlicense.org)\n" \
    "This is free and unencumbered software released into the public domain.\n" \
    "Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n" \
    "software, either in source code form or as a compiled binary, for any purpose,\n" \
    "commercial or non-commercial, and by any means.\n" \
    "In jurisdictions that recognize copyright laws, the author or authors of this\n" \
    "software dedicate any and all copyright interest in the software to the public\n" \
    "domain. We make this dedication for the benefit of the public at large and to\n" \
    "the detriment of our heirs and successors. We intend this dedication to be an\n" \
    "overt act of relinquishment in perpetuity of all present and future rights to\n" \
    "this software under copyright law.\n" \
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n" \
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n" \
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n" \
    "AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n" \
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n" \
    "WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n" \
    "\n" \
    "==============================================================================\n" \
    "16. minimp3 -- CC0 / Public Domain\n" \
    "==============================================================================\n" \
    "\n" \
    "minimp3 (third_party/minimp3), written by lieff.\n" \
    "\n" \
    "To the extent possible under law, the author(s) have dedicated all copyright\n" \
    "and related and neighboring rights to this software to the public domain\n" \
    "worldwide. This software is distributed without any warranty.\n" \
    "See <http://creativecommons.org/publicdomain/zero/1.0/>.\n" \
    "\n" \
    "==============================================================================\n" \
    "17. DejaVu Sans (embedded UI font) -- Bitstream Vera / Arev\n" \
    "==============================================================================\n" \
    "\n" \
    "Fonts are © Bitstream (see below). DejaVu changes are in public domain.\n" \
    "Glyphs imported from Arev fonts are © Tavmjong Bah (see below)\n" \
    "\n" \
    "\n" \
    "Bitstream Vera Fonts Copyright\n" \
    "------------------------------\n" \
    "\n" \
    "Copyright © 2003 by Bitstream, Inc. All Rights Reserved. Bitstream Vera is\n" \
    "a trademark of Bitstream, Inc.\n" \
    "\n" \
    "Permission is hereby granted, free of charge, to any person obtaining a copy\n" \
    "of the fonts accompanying this license (\"Fonts\") and associated\n" \
    "documentation files (the \"Font Software\"), to reproduce and distribute the\n" \
    "Font Software, including without limitation the rights to use, copy, merge,\n" \
    "publish, distribute, and/or sell copies of the Font Software, and to permit\n" \
    "persons to whom the Font Software is furnished to do so, subject to the\n" \
    "following conditions:\n" \
    "\n" \
    "The above copyright and trademark notices and this permission notice shall\n" \
    "be included in all copies of one or more of the Font Software typefaces.\n" \
    "\n" \
    "The Font Software may be modified, altered, or added to, and in particular\n" \
    "the designs of glyphs or characters in the Fonts may be modified and\n" \
    "additional glyphs or characters may be added to the Fonts, only if the fonts\n" \
    "are renamed to names not containing either the words \"Bitstream\" or the word\n" \
    "\"Vera\".\n" \
    "\n" \
    "This License becomes null and void to the extent applicable to Fonts or Font\n" \
    "Software that has been modified and is distributed under the \"Bitstream\n" \
    "Vera\" names.\n" \
    "\n" \
    "The Font Software may be sold as part of a larger software package but no\n" \
    "copy of one or more of the Font Software typefaces may be sold by itself.\n" \
    "\n" \
    "THE FONT SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS\n" \
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY,\n" \
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT,\n" \
    "TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME\n" \
    "FOUNDATION BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING\n" \
    "ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,\n" \
    "WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF\n" \
    "THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE\n" \
    "FONT SOFTWARE.\n" \
    "\n" \
    "Except as contained in this notice, the names of Gnome, the Gnome\n" \
    "Foundation, and Bitstream Inc., shall not be used in advertising or\n" \
    "otherwise to promote the sale, use or other dealings in this Font Software\n" \
    "without prior written authorization from the Gnome Foundation or Bitstream\n" \
    "Inc., respectively. For further information, contact: fonts at gnome dot\n" \
    "org.\n" \
    "\n" \
    "Arev Fonts Copyright\n" \
    "------------------------------\n" \
    "\n" \
    "Copyright © 2006 by Tavmjong Bah. All Rights Reserved.\n" \
    "\n" \
    "Permission is hereby granted, free of charge, to any person obtaining\n" \
    "a copy of the fonts accompanying this license (\"Fonts\") and\n" \
    "associated documentation files (the \"Font Software\"), to reproduce\n" \
    "and distribute the modifications to the Bitstream Vera Font Software,\n" \
    "including without limitation the rights to use, copy, merge, publish,\n" \
    "distribute, and/or sell copies of the Font Software, and to permit\n" \
    "persons to whom the Font Software is furnished to do so, subject to\n" \
    "the following conditions:\n" \
    "\n" \
    "The above copyright and trademark notices and this permission notice\n" \
    "shall be included in all copies of one or more of the Font Software\n" \
    "typefaces.\n" \
    "\n" \
    "The Font Software may be modified, altered, or added to, and in\n" \
    "particular the designs of glyphs or characters in the Fonts may be\n" \
    "modified and additional glyphs or characters may be added to the\n" \
    "Fonts, only if the fonts are renamed to names not containing either\n" \
    "the words \"Tavmjong Bah\" or the word \"Arev\".\n" \
    "\n" \
    "This License becomes null and void to the extent applicable to Fonts\n" \
    "or Font Software that has been modified and is distributed under the \n" \
    "\"Tavmjong Bah Arev\" names.\n" \
    "\n" \
    "The Font Software may be sold as part of a larger software package but\n" \
    "no copy of one or more of the Font Software typefaces may be sold by\n" \
    "itself.\n" \
    "\n" \
    "THE FONT SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n" \
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF\n" \
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT\n" \
    "OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL\n" \
    "TAVMJONG BAH BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n" \
    "INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL\n" \
    "DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n" \
    "FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM\n" \
    "OTHER DEALINGS IN THE FONT SOFTWARE.\n" \
    "\n" \
    "Except as contained in this notice, the name of Tavmjong Bah shall not\n" \
    "be used in advertising or otherwise to promote the sale, use or other\n" \
    "dealings in this Font Software without prior written authorization\n" \
    "from Tavmjong Bah. For further information, contact: tavmjong @ free\n" \
    ". fr.\n" \
    "\n" \
    "TeX Gyre DJV Math\n" \
    "-----------------\n" \
    "Fonts are © Bitstream (see below). DejaVu changes are in public domain.\n" \
    "\n" \
    "Math extensions done by B. Jackowski, P. Strzelczyk and P. Pianowski\n" \
    "(on behalf of TeX users groups) are in public domain.\n" \
    "\n" \
    "Letters imported from Euler Fraktur from AMSfonts are © American\n" \
    "Mathematical Society (see below).\n" \
    "Bitstream Vera Fonts Copyright\n" \
    "Copyright © 2003 by Bitstream, Inc. All Rights Reserved. Bitstream Vera\n" \
    "is a trademark of Bitstream, Inc.\n" \
    "\n" \
    "Permission is hereby granted, free of charge, to any person obtaining a copy\n" \
    "of the fonts accompanying this license (“Fonts”) and associated\n" \
    "documentation\n" \
    "files (the “Font Software”), to reproduce and distribute the Font Software,\n" \
    "including without limitation the rights to use, copy, merge, publish,\n" \
    "distribute,\n" \
    "and/or sell copies of the Font Software, and to permit persons  to whom\n" \
    "the Font Software is furnished to do so, subject to the following\n" \
    "conditions:\n" \
    "\n" \
    "The above copyright and trademark notices and this permission notice\n" \
    "shall be\n" \
    "included in all copies of one or more of the Font Software typefaces.\n" \
    "\n" \
    "The Font Software may be modified, altered, or added to, and in particular\n" \
    "the designs of glyphs or characters in the Fonts may be modified and\n" \
    "additional\n" \
    "glyphs or characters may be added to the Fonts, only if the fonts are\n" \
    "renamed\n" \
    "to names not containing either the words “Bitstream” or the word “Vera”.\n" \
    "\n" \
    "This License becomes null and void to the extent applicable to Fonts or\n" \
    "Font Software\n" \
    "that has been modified and is distributed under the “Bitstream Vera”\n" \
    "names.\n" \
    "\n" \
    "The Font Software may be sold as part of a larger software package but\n" \
    "no copy\n" \
    "of one or more of the Font Software typefaces may be sold by itself.\n" \
    "\n" \
    "THE FONT SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS\n" \
    "OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY,\n" \
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT,\n" \
    "TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME\n" \
    "FOUNDATION\n" \
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING ANY GENERAL,\n" \
    "SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WHETHER IN AN\n" \
    "ACTION\n" \
    "OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF THE USE OR\n" \
    "INABILITY TO USE\n" \
    "THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE FONT SOFTWARE.\n" \
    "Except as contained in this notice, the names of GNOME, the GNOME\n" \
    "Foundation,\n" \
    "and Bitstream Inc., shall not be used in advertising or otherwise to promote\n" \
    "the sale, use or other dealings in this Font Software without prior written\n" \
    "authorization from the GNOME Foundation or Bitstream Inc., respectively.\n" \
    "For further information, contact: fonts at gnome dot org.\n" \
    "\n" \
    "AMSFonts (v. 2.2) copyright\n" \
    "\n" \
    "The PostScript Type 1 implementation of the AMSFonts produced by and\n" \
    "previously distributed by Blue Sky Research and Y&Y, Inc. are now freely\n" \
    "available for general use. This has been accomplished through the\n" \
    "cooperation\n" \
    "of a consortium of scientific publishers with Blue Sky Research and Y&Y.\n" \
    "Members of this consortium include:\n" \
    "\n" \
    "Elsevier Science IBM Corporation Society for Industrial and Applied\n" \
    "Mathematics (SIAM) Springer-Verlag American Mathematical Society (AMS)\n" \
    "\n" \
    "In order to assure the authenticity of these fonts, copyright will be\n" \
    "held by\n" \
    "the American Mathematical Society. This is not meant to restrict in any way\n" \
    "the legitimate use of the fonts, such as (but not limited to) electronic\n" \
    "distribution of documents containing these fonts, inclusion of these fonts\n" \
    "into other public domain or commercial font collections or computer\n" \
    "applications, use of the outline data to create derivative fonts and/or\n" \
    "faces, etc. However, the AMS does require that the AMS copyright notice be\n" \
    "removed from any derivative versions of the fonts which have been altered in\n" \
    "any way. In addition, to ensure the fidelity of TeX documents using Computer\n" \
    "Modern fonts, Professor Donald Knuth, creator of the Computer Modern faces,\n" \
    "has requested that any alterations which yield different font metrics be\n" \
    "given a different name.\n" \
    "\n" \
    "$Id$\n" \
    ""

#endif // PSYMP3_THIRD_PARTY_LICENSES_H
