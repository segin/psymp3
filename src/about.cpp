/*
 * about.cpp - Print about info to either dialog box or console
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psymp3.h"

static char _about_message[] = "This is PsyMP3 version " PSYMP3_VERSION ".\n"\
            "\n"
            "PsyMP3 is free software. You may redistribute and/or modify it under\n"
            "the terms of the GNU General Public License, either version 2,\n"
            "<http://www.gnu.org/licenses/gpl-2.0.html>, or at your option,\n"
            "any later version.\n"
            "\n"
            "PsyMP3 is distributed in the hope that it will be useful, but WITHOUT\n"
            "ANY WARRANTY, not even the implied warranty of MERCHANTABILITY or\n"
            "FIRNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License\n"
            "for details.\n"
            "\n"
            "You should have recieved a copy of the GNU General Public license along\n"
            "with PsyMP3; if not, write to the Free Software Foundation, Inc.,\n"
            "51 Franklin St., Fifth Floor, Boston, MA 02110-1301, USA.\n"
            "\n"
            "Written by " PSYMP3_MAINTAINER "\n";

void about_console()
{
    std::cout << _about_message << std::endl;
}
