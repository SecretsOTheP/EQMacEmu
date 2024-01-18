/**
 * EQEmulator: Everquest Server Emulator
 * Copyright (C) 2001-2020 EQEmulator Development Team (https://github.com/EQEmu/Server)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY except by those people which sell it, which
 * are required to give you total support for your newly bought product;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
*/


#include <string>
#include <sys/stat.h>

#ifdef _WINDOWS
#include <direct.h>
#endif

#include "file_util.h"

/**
 * @param name
 * @return
 */
bool FileUtil::exists(const std::string& name)
{
	struct stat st {};
	if (stat(name.c_str(), &st) == 0) { // exists
		return true;
	}
	return false;
}

/**
 * @param directory_name
 */
void FileUtil::mkdir(const std::string& directory_name)
{
	bool dir_exists = exists(directory_name);
#ifdef _WINDOWS
	if (dir_exists)
		return;
	_mkdir(directory_name.c_str());
#else
	if (dir_exists) {
		return;
	}
	::mkdir(directory_name.c_str(), 0755);
#endif
}