/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include "storage/AppUtil.h"
#include "storage/Regex.h"
#include "storage/EtcRaidtab.h"
#include "storage/AsciiFile.h"
#include "storage/Md.h"
#include "storage/MdPartCo.h"
#include "storage/StorageTypes.h"


namespace storage
{
    using namespace std;


    EtcRaidtab::EtcRaidtab(const Storage* sto, const string& prefix)
	: sto(sto), mdadm(prefix + "/etc/mdadm.conf")
    {
	buildMdadmMap();
	buildMdadmMap2();
    }


EtcRaidtab::~EtcRaidtab()
{
}


void 
EtcRaidtab::updateEntry(unsigned num, const string& mline)
    {
    y2mil("num:" << num);
    string dline;
    map<unsigned, entry>::const_iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm.replace( i->second.first, i->second.last-i->second.first+1, mline );
	}
    else
	{
	mdadm.append( mline );
	}

    setDeviceLine("DEVICE partitions");

    if (sto->hasIScsiDisks())
       setAutoLine("AUTO -all");

    updateMdadmFile();
    }


bool EtcRaidtab::updateEntry(const mdconf_info& info)
{
  y2mil("Called. UUID="<<info.md_uuid<<", fs_name="<<info.fs_name);

  if( info.md_uuid.empty() )
    {
    y2mil("Empty Md UUID!");
    return false;
    }
  if( info.container_present )
    {
    if( info.container_info.md_uuid.empty() )
      {
      y2mil("Empty Md UUID for Container");
      return false;
      }
    }

  if( info.container_present )
    {
    updateContainer(info);
    }
  //after container.
  map<string, entry>::const_iterator i = uuidtab.find(info.md_uuid);
  if( i != uuidtab.end() )
    {
    mdadm.replace( i->second.first,
                    i->second.last - i->second.first + 1,
                    ArrayLine(info) );
    }
  else
    {
    mdadm.append(ArrayLine(info));
    }
  updateMdadmFile();
  return true;
}


    void
    EtcRaidtab::setDeviceLine(const string& line)
    {
	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator i = find_if(lines, string_starts_with("DEVICE"));
	if (i == lines.end())
	    mdadm.insert(0, line);
	else
	    mdadm.replace(distance(lines.begin(), i), 1, line);
    }


    void
    EtcRaidtab::setAutoLine(const string& line)
    {
	const vector<string>& lines = mdadm.lines();
	vector<string>::const_iterator i = find_if(lines, string_starts_with("AUTO"));
	if (i == lines.end())
	    mdadm.insert(0, line);
	else
	    mdadm.replace(distance(lines.begin(), i), 1, line);
    }


bool EtcRaidtab::updateContainer(const mdconf_info& info)
{
  while( true )
  {
  map<string,entry>::const_iterator i =  uuidtab.find(info.md_uuid);
  map<string,entry>::const_iterator i2 = uuidtab.find(info.container_info.md_uuid);

  if( i == uuidtab.end() )
    {
    // No volume yet.
    if(i2 == uuidtab.end())
      {
      // No container yet.
      mdadm.append(ContLine(info));
      updateMdadmFile();
      return true;
      }
    // No Volume, but there is already Container.
    mdadm.replace( i2->second.first,
                    i2->second.last - i2->second.first + 1,
                    ContLine(info) );
    updateMdadmFile();
    return true;
    }
  //There is Volume line.
  if(i2 == uuidtab.end())
    {
    //There is Volume line while no Container line.
    //Insert line and make another iteration.
    mdadm.insert(i->second.first,ContLine(info));
    updateMdadmFile();
    continue;
    }
  else
    {
    //there is a line for container and for volume.
    if( i2->second.first >= i->second.first )
      {
      // error. Container must be first
      mdadm.remove(i2->second.first,
                    i2->second.last- i2->second.first + 1);
      // and make another iteration.
      updateMdadmFile();
      continue;
      }
    else
      {
      //Ok, container is before volume
      mdadm.replace( i2->second.first,
                      i2->second.last - i2->second.first + 1,
                      ContLine(info) );
      updateMdadmFile();
      return true;
      }
    }
  }
}

string EtcRaidtab::ContLine(const mdconf_info& info)
{
  string s = "ARRAY metadata=" + info.container_info.metadata + " UUID=" + info.container_info.md_uuid;
  return s;
}


string EtcRaidtab::ArrayLine(const mdconf_info& info)
{
  string line = "ARRAY " + info.fs_name;

  if( info.container_present )
    {
    line = line + " container=" + info.container_info.md_uuid;
    line = line + " member=" + info.member;
    }
  line = line + " UUID=" + info.md_uuid;
  return line;
}

//2 cases: remove volume or container
//No check if container is used by other Volume.
bool EtcRaidtab::removeEntry(const mdconf_info& info)
{
  map<string, entry>::const_iterator i;
  if( info.md_uuid.empty() )
    {
    if( info.container_present == false)
      {
      return false;
      }
    if( info.container_info.md_uuid.empty() )
      {
      return false;
      }
    i = uuidtab.find( info.container_info.md_uuid );
    }
  else
    {
     i = uuidtab.find( info.md_uuid );
    }
  if( i != uuidtab.end() )
    {
    mdadm.remove( i->second.first, i->second.last-i->second.first+1 );
    updateMdadmFile();
    }
  return true;
}

void
EtcRaidtab::removeEntry( unsigned num )
    {
    map<unsigned, entry>::const_iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm.remove( i->second.first, i->second.last-i->second.first+1 );
	updateMdadmFile();
	}
    }

void
EtcRaidtab::updateMdadmFile()
    {
    mtab.clear();
    uuidtab.clear();
    mdadm.save();
    buildMdadmMap();
    buildMdadmMap2();
    }

void
EtcRaidtab::buildMdadmMap()
    {
    unsigned lineno = 0;
    unsigned mdnum;
    entry e;
    while (lineno < mdadm.numLines())
	{
	string key = extractNthWord( 0, mdadm[lineno] );
	if( key == "ARRAY" &&
		 Md::mdStringNum( extractNthWord( 1, mdadm[lineno] ), mdnum ))
	    {
	    e.first = lineno++;
	    while( lineno < mdadm.numLines() && 
	           (key = extractNthWord( 0, mdadm[lineno] ))!="ARRAY" &&
		   key != "DEVICE" && key != "AUTO")
		{
		key = extractNthWord( 0, mdadm[lineno++] );
		}
	    e.last = lineno-1;
	    mtab[mdnum] = e;
	    }
	else
	    lineno++;
	}
    }

void
EtcRaidtab::buildMdadmMap2()
{
  unsigned lineno = 0;
  entry e;
  for(lineno=0; lineno < mdadm.numLines(); lineno++ )
    {
    if( getLineType(mdadm[lineno]) == EtcRaidtab::ARRAY )
      {
      unsigned endl = lineno;
      string uuid;
      if( getArrayLine(endl,uuid) )
        {
        if(uuid.empty() )
          {
          // no UUID
          continue;
          }
        e.first = lineno;
        e.last = endl-1;
        uuidtab[uuid] = e;
        lineno = endl-1;
        }
      else
        {
        //
        }
      }
    else
      {
      //
      }
    }
}

string
EtcRaidtab::getUUID(const string& line) const
{
  string::size_type pos = line.find("UUID=");
  if( pos != string::npos )
    {
    //switch 'uuid='
    pos += 5;
    // 128bit - represented as 32 hex digits + 3 x ':'.
    return line.substr(pos,pos+35);
    }
  else
    {
    return "";
    }
}

EtcRaidtab::lineType
EtcRaidtab::getLineType(const string& line) const
{
  if( line.empty() )
    {
    return EtcRaidtab::EMPTY;
    }
  string type = extractNthWord(0,line);
  if( type == "DEVICE")
    {
    return EtcRaidtab::DEVICE;
    }
  if( type == "ARRAY")
    {
    return EtcRaidtab::ARRAY;
    }
  if( type == "MAILFROM")
    {
    return EtcRaidtab::MAILFROM;
    }
  if( type == "PROGRAM")
    {
    return EtcRaidtab::PROGRAM;
    }
  if( type == "CREATE")
    {
    return EtcRaidtab::CREATE;
    }
  if( type == "HOMEHOST")
    {
    return EtcRaidtab::HOMEHOST;
    }
  if( type == "AUTO")
    {
    return EtcRaidtab::AUTO;
    }
  if( type.find("#") == 0 )
    {
    return EtcRaidtab::COMMENT;
    }
  return EtcRaidtab::UNKNOWN;
}

bool EtcRaidtab::getArrayLine(unsigned& line, string& uuid)
{
  if( getLineType(mdadm[line]) != EtcRaidtab::ARRAY )
    {
    return false;
    }
  uuid = getUUID(mdadm[line]);
  while( true )
    {
    line++;
    if( line >= mdadm.numLines() )
      {
      if( uuid.empty() )
        return false;
      else
        return true;
      }
    EtcRaidtab::lineType lt = getLineType(mdadm[line]);
    if( lt == EtcRaidtab::ARRAY    || lt == EtcRaidtab::DEVICE   ||
        lt == EtcRaidtab::AUTO     || lt == EtcRaidtab::CREATE   ||
        lt == EtcRaidtab::HOMEHOST || lt == EtcRaidtab::MAILFROM ||
        lt == EtcRaidtab::PROGRAM)
      {
      return true;
      }
    if( lt == EtcRaidtab::COMMENT ||
        lt == EtcRaidtab::EMPTY )
      {
      continue;
      }
    if( lt == EtcRaidtab::UNKNOWN )
      {
      // possibly following line. Check for x=y pair.
      if( (mdadm[line]).find("=") == string::npos )
        {
        //kind of error.
        return false;
        }
      //Check for UUID
      if( uuid.empty() )
        {
        uuid = getUUID(mdadm[line]);
        }
      continue;
      }
    }
}

}
