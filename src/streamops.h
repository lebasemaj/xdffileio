/*
	Copyright (C) 2010  EPFL (Ecole Polytechnique Fédérale de Lausanne)
	Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef STREAMOPS_H
#define STREAMOPS_H

XDF_LOCAL int read16bval(FILE* file, unsigned int num, void* value);
XDF_LOCAL int write16bval(FILE* file, unsigned int num, const void* value);
XDF_LOCAL int read32bval(FILE* file, unsigned int num, void* value);
XDF_LOCAL int write32bval(FILE* file, unsigned int num, const void* value);
XDF_LOCAL int read64bval(FILE* file, unsigned int num, void* value);
XDF_LOCAL int write64bval(FILE* file, unsigned int num, const void* value);
XDF_LOCAL int read_double_field(FILE* file, double* val, unsigned int len);
XDF_LOCAL int read_int_field(FILE* file, int* val, unsigned int len);
XDF_LOCAL int read_string_field(FILE* file, char* val, unsigned int len);

#endif /* STREAMOPS_H */
