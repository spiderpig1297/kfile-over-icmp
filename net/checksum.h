/**
 * This file is part of the khidden-file-sender module (https://github.com/spiderpig1297/khidden-file-sender).
 * Copyright (c) 2020 Idan Ofek.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <linux/types.h>

__sum16 icmp_csum(const void* icmp_layer, size_t length);
