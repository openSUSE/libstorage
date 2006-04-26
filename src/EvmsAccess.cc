/*
  Textdomain    "storage"
*/

#include <dlfcn.h>
#include <iterator>
#include <string>
#include <iostream>

#include "y2storage/AppUtil.h"
#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTmpl.h"
#include "Md.h"
#include "Regex.h"
#include "AsciiFile.h"
#include "EvmsAccess.h"

using namespace std;
using namespace storage;

EvmsObject::EvmsObject( object_handle_t obid ) 
    {
    int ret;
    init();
    idt = obid;
    if( (ret = evms_get_info( idt, &info_p)!=0 ))
	{
	y2error( "error %d getting info for object:%d", ret, idt );
	}
    else
	{
	switch( info_p->type )
	    {
	    case ::DISK:
		typ = EVMS_DISK;
		nam = info_p->info.disk.name;
		break;
	    case SEGMENT:
		typ = EVMS_SEGMENT;
		nam = info_p->info.segment.name;
		break;
	    case REGION:
		typ = EVMS_REGION;
		nam = info_p->info.region.name;
		break;
	    case CONTAINER:
		typ = EVMS_CONTAINER;
		nam = info_p->info.container.name;
		break;
	    case VOLUME:
		typ = EVMS_VOLUME;
		nam = info_p->info.volume.name;
		break;
	    case PLUGIN:
		typ = EVMS_PLUGIN;
		nam = info_p->info.plugin.short_name;
		break;
	    case EVMS_OBJECT:
		typ = EVMS_OBJ;
		nam = info_p->info.object.name;
		break;
	    default:
		break;
	    }
	}
    }

EvmsObject::~EvmsObject()
    {
    if( own_ptr && info_p )
	{
	evms_free( info_p );
	}
    info_p = 0;
    }

void EvmsObject::init()
    {
    idt = 0;
    is_data = is_freespace = false;
    size = 0;
    info_p = NULL;
    typ = EVMS_UNKNOWN;
    own_ptr = true;
    nam.clear();
    }

bool EvmsObject::isDataType() const
    {
    return( typ==EVMS_DISK || typ==EVMS_SEGMENT || typ==EVMS_REGION ||
            typ==EVMS_OBJ );
    }

void EvmsObject::output( ostream &str ) const
    {
    str << type();
    if( isData() )
	{
	str << " D";
	}
    else if( isFreespace() )
	{
	str << " F";
	}
    str << " id:" << id() << " name:" << name();
    }

namespace storage
{

ostream& operator<<( ostream &str, const EvmsObject& obj )
    {
    obj.output( str );
    str << endl;
    return( str );
    }

ostream& operator<<( ostream &str, ObjType obj )
    {
    switch( obj )
	{
	case EVMS_DISK:
	    str << "DSK";
	    break;
	case EVMS_SEGMENT:
	    str << "SEG";
	    break;
	case EVMS_REGION:
	    str << "REG";
	    break;
	case EVMS_CONTAINER:
	    str << "CNT";
	    break;
	case EVMS_VOLUME:
	    str << "VOL";
	    break;
	case EVMS_PLUGIN:
	    str << "PLG";
	    break;
	case EVMS_UNKNOWN:
	default:
	    str << "UNK";
	    break;
	}
    return( str );
    }

}

EvmsDataObject::EvmsDataObject( EvmsObject *const obj ) 
    {
    init();
    *(EvmsObject*)this = *obj;
    obj->disownPtr();
    storage_object_info_t* sinfo_p = getInfop();
    if( sinfo_p )
	{
	size = sinfo_p->size/2;
	if( sinfo_p->data_type==DATA_TYPE )
	    {
	    is_data = true;
	    }
	else if( sinfo_p->data_type==FREE_SPACE_TYPE )
	    {
	    is_freespace = true;
	    }
	}
    else
	{
	y2error( "invalid constructing data object %d", obj->id() );
	}
    }

EvmsDataObject::EvmsDataObject( object_handle_t id ) : EvmsObject(id)
    {
    EvmsDataObject( (EvmsObject*)this );
    own_ptr = true;
    }

void EvmsDataObject::init()
    {
    consumed = NULL;
    vol = NULL;
    }

storage_object_info_t* EvmsDataObject::getInfop()
    {
    storage_object_info_t* sinfo_p = NULL;
    if( info_p )
	{
	switch( type() )
	    {
	    case EVMS_DISK:
		sinfo_p = &info_p->info.disk;
		break;
	    case EVMS_SEGMENT:
		sinfo_p = &info_p->info.segment;
		break;
	    case EVMS_REGION:
		sinfo_p = &info_p->info.region;
		break;
	    case EVMS_OBJ:
		sinfo_p = &info_p->info.object;
		break;
	    default:
		break;
	    }
	}
    return( sinfo_p );
    }

void EvmsDataObject::addRelation( EvmsAccess* Acc )
    {
    storage_object_info_t* sinfo_p = getInfop();
    if( sinfo_p )
	{
	if( sinfo_p->consuming_container>0 )
	    {
	    consumed = Acc->addObject( sinfo_p->consuming_container );
	    }
	if( sinfo_p->volume>0 )
	    {
	    vol = Acc->addObject( sinfo_p->volume );
	    }
	}
    }

void EvmsDataObject::output( ostream& str ) const
    {
    ((EvmsObject*)this)->output( str );
    str << " size:" << sizeK();
    if( consumedBy()!=NULL )
	{
        str << " cons:" << consumedBy()->id();
	}
    if( volume()!=NULL )
	{
	str << " vol:" << volume()->id();
	}
    }

namespace storage
{

ostream& operator<<( ostream &str, const EvmsDataObject& obj )
    {
    obj.output( str );
    str << endl;
    return( str );
    }

}

EvmsContainerObject::EvmsContainerObject( EvmsObject *const obj ) 
    {
    init();
    *(EvmsObject*)this = *obj;
    obj->disownPtr();
    storage_container_info_t* cinfo_p = getInfop();
    if( cinfo_p )
	{
	size = cinfo_p->size/2;
	extended_info_array_t *info_p = NULL;
	int ret = evms_get_extended_info( idt, NULL, &info_p );
	if( ret == 0 && info_p != NULL )
	    {
	    for( unsigned i=0; i<info_p->count; i++ )
		{
		if( strcmp( info_p->info[i].name, "PE_Size" )==0 )
		    pe_size = info_p->info[i].value.ui32/2;
		else if( strcmp( info_p->info[i].name, "Extent_Size" )==0 )
		    {
		    pe_size = info_p->info[i].value.ui64/2;
		    lvm1 = false;
		    }
		else if( strcmp( info_p->info[i].name, "VG_UUID" )==0 )
		    uuid = info_p->info[i].value.s;
		else if( strcmp( info_p->info[i].name, "UUID" )==0 )
		    uuid = info_p->info[i].value.s;
		}
	    if( pe_size == 0 )
		{
		y2error( "cannot determine PE size of %d:%s", idt, name().c_str() );
		}
	    evms_free( info_p );
	    }
	else
	    {
	    y2error( "cannot get extended info of %d:%s", idt, name().c_str() );
	    }
	}
    else
	{
	y2error( "invalid constructing container object %d", obj->id() );
	}
    }

EvmsContainerObject::EvmsContainerObject( object_handle_t id ) : EvmsObject(id)
    {
    EvmsContainerObject( (EvmsObject*)this );
    own_ptr = true;
    }

void EvmsContainerObject::init()
    {
    creat.clear();
    cons.clear();
    ctype.clear();
    free = 0;
    pe_size = 0;
    lvm1 = true;
    lvm = false;
    }

storage_container_info_t* EvmsContainerObject::getInfop()
    {
    storage_container_info_t *sinfo_p = NULL;
    if( info_p && type()==EVMS_CONTAINER )
	{
	sinfo_p = &info_p->info.container;
	}
    return( sinfo_p );
    }

void EvmsContainerObject::addRelation( EvmsAccess* Acc )
    {
    storage_container_info_t* sinfo_p = getInfop();
    if( sinfo_p )
	{
	for( unsigned i=0; i<sinfo_p->objects_consumed->count; i++ )
	    {
	    peinfo pe;
	    pe.obj = Acc->addObject( sinfo_p->objects_consumed->handle[i] );
	    cons.push_back( pe );
	    }
	for( unsigned i=0; i<sinfo_p->objects_consumed->count; i++ )
	    {
	    string qname;
	    if( lvm1 )
		qname = "PV"+decString(i+1);
	    else
		qname = "Object"+decString(i);
	    extended_info_array_t *info_p = NULL;
	    int ret = evms_get_extended_info( idt, const_cast<char*>(qname.c_str()), &info_p );
	    if( ret == 0 && info_p != NULL )
		{
		string uuid;
		string name;
		unsigned long long total = 0;
		unsigned long long free = 0;

		for( unsigned j=0; j<info_p->count; j++ )
		    {
		    if( strcmp( info_p->info[j].name, "Available_PEs" )==0 )
			free = info_p->info[j].value.ui32;
		    else if( strcmp( info_p->info[j].name, "Freespace" )==0 )
			free = info_p->info[j].value.ui64/2/pe_size;
		    else if( strncmp( info_p->info[j].name, "PEMapPV", 7 )==0 )
			total = info_p->info[j].value.ui32;
		    else if( strcmp( info_p->info[j].name, "Extents" )==0 )
			total = info_p->info[j].value.ui64;
		    else if( strcmp( info_p->info[j].name, "PV_Name" )==0 )
			name = info_p->info[j].value.s;
		    else if( strcmp( info_p->info[j].name, "Name" )==0 )
			name = info_p->info[j].value.s;
		    else if( strcmp( info_p->info[j].name, "PV_UUID" )==0 )
			uuid = info_p->info[j].value.s;
		    else if( strcmp( info_p->info[j].name, "UUID" )==0 )
			uuid = info_p->info[j].value.s;
		    }
		list<peinfo>::iterator pe = cons.begin();
		while( pe!=cons.end() && pe->obj->name()!=name )
		    ++pe;
		if( pe!=cons.end() )
		    {
		    pe->uuid = uuid;
		    pe->size = total;
		    pe->free = free;
		    }
		else
		    y2error( "name %s not found in list", name.c_str() );
		evms_free( info_p );
		}
	    else
		{
		y2error( "cannot get extended info %s of %d:%s", 
		         qname.c_str(), idt, name().c_str() );
		}
	    }
	for( unsigned i=0; i<sinfo_p->objects_produced->count; i++ )
	    {
	    EvmsObject* obj = 
		Acc->addObject( sinfo_p->objects_produced->handle[i] );
	    if( obj->isData() )
		{
		creat.push_back( obj );
		}
	    else if( obj->isFreespace() )
		{
		free = free + obj->sizeK();
		}
	    }
	if( sinfo_p->plugin>0 )
	    {
	    EvmsObject *plugin = Acc->addObject( sinfo_p->plugin );
	    y2mil( "plugin id:" << sinfo_p->plugin 
	           << " name:" << plugin->name() );
	    ctype = plugin->name();
	    string tmp = ctype;
	    tolower(tmp);
	    lvm = (tmp == "lvm2") || (tmp == "lvmregmgr");
	    if( lvm )
		{
		lvm1 = tmp == "lvmregmgr";
		}
	    y2mil( "isLvm:" << isLvm() << " isLvm1:" << isLvm1() );
	    }
	}
    }

void EvmsContainerObject::output( ostream& str ) const
    {
    ((EvmsObject*)this)->output( str );
    str << " size:" << sizeK() 
        << " free:" << freeK()
        << " pesize:" << peSize()
	<< " type:" << typeName()
	<< " uuid:" << uuid;
    if( creat.size()>0 )
	{
	str << " creates:<";
	for( list<EvmsObject *>::const_iterator i=creat.begin();
	     i!=creat.end(); i++ )
	    {
	    if( i!=creat.begin() )
		str << " ";
	    str << (*i)->id();
	    }
	str << ">";
	}
    if( cons.size()>0 )
	{
	str << " consumes:<";
	for( list<peinfo>::const_iterator i=cons.begin();
	     i!=cons.end(); i++ )
	    {
	    if( i!=cons.begin() )
		str << " ";
	    str << i->obj->id() << "," << i->size << "," << i->free << "," << i->uuid;
	    }
	str << ">";
	}
    }

namespace storage
{

ostream& operator<<( ostream &str, const EvmsContainerObject& obj )
    {
    obj.output( str );
    str << endl;
    return( str );
    }

}

EvmsVolumeObject::EvmsVolumeObject( EvmsObject *const obj ) 
    {
    init();
    *(EvmsObject*)this = *obj;
    obj->disownPtr();
    logical_volume_info_s* vinfo_p = getInfop();
    if( vinfo_p )
	{
	size = vinfo_p->vol_size/2;
	dev = nam;
	nam.erase( 0, 10 );
	nat = !(vinfo_p->flags & VOLFLAG_COMPATIBILITY);
	act = vinfo_p->flags & VOLFLAG_ACTIVE;
	}
    else
	{
	y2error( "invalid constructing volume object %d", obj->id() );
	}
    }

EvmsVolumeObject::EvmsVolumeObject( object_handle_t id ) : EvmsObject(id)
    {
    EvmsVolumeObject( (EvmsObject*)this );
    own_ptr = true;
    }

void EvmsVolumeObject::init()
    {
    dev.clear();
    nat = false;
    act = false;
    assc = NULL;
    consumed = NULL;
    cons = NULL;
    }

logical_volume_info_s* EvmsVolumeObject::getInfop()
    {
    logical_volume_info_s *sinfo_p = NULL;
    if( info_p && type()==EVMS_VOLUME )
	{
	sinfo_p = &info_p->info.volume;
	}
    return( sinfo_p );
    }

void EvmsVolumeObject::addRelation( EvmsAccess* Acc )
    {
    logical_volume_info_s* sinfo_p = getInfop();
    if( sinfo_p )
	{
	if( sinfo_p->object>0 )
	    {
	    cons = Acc->addObject( sinfo_p->object );
	    }
#if 0
	if( sinfo_p->associated_volume>0 )
	    {
	    assc = Acc->addObject( sinfo_p->associated_volume );
	    }
#endif
	}
    }

void EvmsVolumeObject::setConsumedBy( EvmsObject* Obj )
    {
    if( consumedBy()!=NULL )
        {
	y2error( "object %d consumed twice %d and %d", id(), consumedBy()->id(),
	         Obj->id() );
        }   
    else
        {
        consumed = Obj;
        }
    }

void EvmsVolumeObject::output( ostream& str ) const
    {
    ((EvmsObject*)this)->output( str );
    str << " size:" << sizeK() 
	<< " device:" << device()
	<< " native:" << native();
    if( consumes()!=NULL )
	{
	str << " consumes:" << consumes()->id();
	}
    if( consumedBy()!=NULL )
	{
	str << " consumed:" << consumedBy()->id();
	}
    if( assVol()!=NULL )
	{
	str << " assc:" << assVol()->id();
	}
    }

namespace storage
{

ostream& operator<<( ostream &str, const EvmsVolumeObject& obj )
    {
    obj.output( str );
    str << endl;
    return( str );
    }

}

int EvmsAccess::pluginFilterFunction( const char* plugin )
    {
    static char *ExcludeList[] = { "/ext2-", "/reiser-", "/jfs-", "/xfs-", 
				   "/swap-", "/ocfs2-", "/ntfs-" };
    int ret = 0;
    unsigned i = 0;
    while( !ret && i<sizeof(ExcludeList)/sizeof(char*) )
	{
	ret = strstr( plugin, ExcludeList[i] )!=NULL;
	i++;
	}
    y2milestone( "plugin %s ret:%d", plugin, ret );
    return( ret );
    }

EvmsAccess::EvmsAccess() : EvmsOpen_b(false)
    {
    y2debug( "begin Konstruktor EvmsAccess" );
    if( !runningFromSystem() )
	{
	unlink( "/var/lock/evms-engine" );
	}
    evms_set_load_plugin_fct( pluginFilterFunction );
    int ret = evms_open_engine( NULL, (engine_mode_t)ENGINE_READWRITE, NULL, 
                                DEFAULT, NULL );
    y2debug( "evms_open_engine ret %d", ret );
    if( ret != 0 )
	{
	y2error( "evms_open_engine evms_strerror %s", evms_strerror(ret));
	}
    else
	{
	EvmsOpen_b = true;
	rereadAllObjects();
	}
    y2debug( "End Konstruktor EvmsAccess" );
    y2mil( "ret:" << endl << *this );
    }


int EvmsAccess::activate()
    {
    evms_close_engine();
    string line;
    debug_level_t log_level = DEFAULT;
    AsciiFile conf( "/etc/evms.conf" );
    Regex rx( "[ \t]*debug_level[ \t]*=[ \t]*" );
    int ret = conf.find( 0, rx );
    y2mil( "debug line:" << ret );
    if( ret>=0 )
	{
	list<string> ls = splitString( conf[ret], " \t=" );
	y2mil( "debug line:" << conf[ret] << " list:" << ls );
	if( ls.size()>=2 )
	    {
	    list<string>::iterator i = ls.begin();
	    ++i;
	    if( *i == "critical" )
		log_level = CRITICAL;
	    else if( *i == "serious" )
		log_level = SERIOUS;
	    else if( *i == "error" )
		log_level = ERROR;
	    else if( *i == "warning" )
		log_level = WARNING;
	    else if( *i == "details" )
		log_level = DETAILS;
	    else if( *i == "entry_exit" )
		log_level = ENTRY_EXIT;
	    else if( *i == "debug" )
		log_level = DEBUG;
	    else if( *i == "extra" )
		log_level = EXTRA;
	    else if( *i == "everything" )
		log_level = EVERYTHING;
	    y2mil( "level string:\"" << *i << "\" val:" << log_level );
	    }
	}
    y2mil( "log level:" << log_level );
    ret = evms_open_engine( NULL, (engine_mode_t)ENGINE_READWRITE, NULL, 
			    log_level, NULL );
    if( ret != 0 )
	{
	y2error( "evms_open_engine evms_strerror %s", evms_strerror(ret));
	}
    else
	{
	ret = evms_commit_changes();
	if( ret != 0 )
	    {
	    y2error( "evms_commit_changes evms_strerror %s", evms_strerror(ret));
	    }
	else
	    {
	    rereadAllObjects();
	    y2mil( "after rereadAllObjects" );
	    y2mil( "ret:" << endl << *this );
	    }
	}
    return( ret );
    }

void EvmsAccess::rereadAllObjects()
    {
    for( list<EvmsObject*>::iterator p=objects.begin(); p!=objects.end(); p++ )
	{
	delete *p;
	}
    objects.clear();

    handle_array_t* handle_p = 0;
    evms_get_object_list( (object_type_t)0, (data_type_t)0, (plugin_handle_t)0,
                          (object_handle_t)0, (object_search_flags_t)0, 
			  &handle_p );
    for( unsigned i=0; i<handle_p->count; i++ )
	{
	addObject( handle_p->handle[i] );
	}
    evms_free( handle_p );
    evms_get_plugin_list( EVMS_REGION_MANAGER, (plugin_search_flags_t)0, 
                          &handle_p );
    for( unsigned i=0; i<handle_p->count; i++ )
	{
	addObject( handle_p->handle[i] );
	}
    evms_free( handle_p );
    addObjectRelations();
    }

void EvmsAccess::addObjectRelations()
    {
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->isDataType() )
	    {
	    (*Ptr_Ci)->addRelation( this );
	    }
	}
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_CONTAINER )
	    {
	    (*Ptr_Ci)->addRelation( this );
	    }
	}
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_VOLUME )
	    {
	    (*Ptr_Ci)->addRelation( this );
	    }
	}
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_CONTAINER )
	    {
	    const list<EvmsContainerObject::peinfo>& cons 
		= ((EvmsContainerObject*)*Ptr_Ci)->consumes();
	    for( list<EvmsContainerObject::peinfo>::const_iterator i=cons.begin(); 
	         i!=cons.end(); i++ )
		{
		if( i->obj->type()==EVMS_VOLUME )
		    {
		    ((EvmsVolumeObject*)i->obj)->setConsumedBy( *Ptr_Ci );
		    }
		}
	    }
	else if( (*Ptr_Ci)->type()==EVMS_VOLUME )
	    {
	    EvmsObject* cons = ((EvmsVolumeObject*)*Ptr_Ci)->consumes();
	    if( cons->type()==EVMS_VOLUME )
		{
		((EvmsVolumeObject*)cons)->setConsumedBy( *Ptr_Ci );
		}
	    }
	}
    }

EvmsAccess::~EvmsAccess()
    {
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	 {
	 delete *Ptr_Ci;
	 }
    if( EvmsOpen_b )
	{
	evms_close_engine();
	EvmsOpen_b = false;
	}
    }

EvmsObject *const EvmsAccess::addObject( object_handle_t id )
    {
    EvmsObject * ret;
    if( (ret=find( id )) == NULL )
	{
	EvmsObject *Obj = new EvmsObject( id );
	ret = Obj;
	y2debug( "id %d type %d", id, Obj->type() );
	switch( Obj->type() )
	    {
	    case EVMS_DISK:
	    case EVMS_SEGMENT:
	    case EVMS_REGION:
	    case EVMS_OBJ:
		{
		EvmsDataObject* Data = new EvmsDataObject( Obj );
		objects.push_back( Data );
		delete Obj;
		ret = Data;
		}
		break;
	    case EVMS_CONTAINER:
		{
		EvmsContainerObject* Cont = new EvmsContainerObject( Obj );
		objects.push_back( Cont );
		delete Obj;
		ret = Cont;
		}
		break;
	    case EVMS_VOLUME:
		{
		EvmsVolumeObject* Vol = new EvmsVolumeObject( Obj );
		if( Vol->active() )
		    {
		    objects.push_back( Vol );
		    ret = Vol;
		    }
		else
		    {
		    delete Vol;
		    ret = NULL;
		    }
		delete Obj;
		}
		break;
	    default:
		objects.push_back( Obj );
		break;
	    }
	}
    return( ret );
    }

EvmsObject *const EvmsAccess::find( object_handle_t id )
    {
    EvmsObject *ret = NULL;
    list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin();
    while( Ptr_Ci != objects.end() && (*Ptr_Ci)->id()!=id )
	{
	Ptr_Ci++;
	}
    if( Ptr_Ci != objects.end() )
	{
	ret = *Ptr_Ci;
	}
    return( ret );
    }

void EvmsAccess::listVolumes( list<const EvmsVolumeObject*>& l ) const
    {
    l.clear();
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin();
	 Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type() == EVMS_VOLUME )
	    {
	    l.push_back( (EvmsVolumeObject*)*Ptr_Ci );
	    }
	}
    y2milestone( "size %zd", l.size() );
    }

void EvmsAccess::listContainer( list<const EvmsContainerObject*>& l ) const
    {
    l.clear();
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin();
	 Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	y2debug( "type %d vol %d", (*Ptr_Ci)->type(), EVMS_CONTAINER );
	if( (*Ptr_Ci)->type() == EVMS_CONTAINER )
	    {
	    l.push_back( (EvmsContainerObject*)*Ptr_Ci );
	    }
	}
    y2milestone( "size %zd", l.size() );
    }

plugin_handle_t EvmsAccess::getLvmPlugin( bool lvm2 )
    {
    plugin_handle_t handle = 0;
    string plname = lvm2?"LVM2":"LvmRegMgr";
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_PLUGIN && (*Ptr_Ci)->name()==plname )
	    {
	    handle = (*Ptr_Ci)->id();
	    }
	}
    y2milestone( "handle %d", handle );
    return( handle );
    }

object_handle_t EvmsAccess::findUsingVolume( object_handle_t id )
    {
    object_handle_t handle = 0;
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_VOLUME && 
	    ((EvmsVolumeObject*)*Ptr_Ci)->consumes()->id()==id )
	    {
	    handle = (*Ptr_Ci)->id();
	    y2mil( "UsingVol:" << *((EvmsVolumeObject*)*Ptr_Ci) );
	    }
	}
    y2milestone( "%d used by handle %d", id, handle );
    return( handle );
    }

const EvmsContainerObject* EvmsAccess::findContainer( const string& name )
    {
    EvmsContainerObject* ret_pi = NULL;
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_CONTAINER && (*Ptr_Ci)->name()==name )
	    {
	    ret_pi = (EvmsContainerObject*)*Ptr_Ci;
	    }
	}
    y2milestone( "Container %s has id %d", name.c_str(), 
                 ret_pi==NULL?0:ret_pi->id() );
    return( ret_pi );
    }

const EvmsDataObject* EvmsAccess::findRegion( const string& container, 
					      const string& name )
    {
    EvmsDataObject* ret_pi = NULL;
    string rname = container + "/" + name;
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_REGION && (*Ptr_Ci)->name()==rname )
	    {
	    ret_pi = (EvmsDataObject*)*Ptr_Ci;
	    }
	}
    y2milestone( "Region %s in Container %s has id %d", name.c_str(), 
                 container.c_str(), ret_pi==NULL?0:ret_pi->id() );
    return( ret_pi );
    }

const EvmsDataObject* EvmsAccess::findSegment( const string& name )
    {
    EvmsDataObject* ret_pi = NULL;
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_SEGMENT && (*Ptr_Ci)->name()==name )
	    {
	    ret_pi = (EvmsDataObject*)*Ptr_Ci;
	    }
	}
    y2milestone( "Segment %s has id %d", name.c_str(), 
                 ret_pi==NULL?0:ret_pi->id() );
    return( ret_pi );
    }

const EvmsVolumeObject* EvmsAccess::findVolume( const string& name )
    {
    EvmsVolumeObject* ret_pi = NULL;
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	if( (*Ptr_Ci)->type()==EVMS_VOLUME && (*Ptr_Ci)->name()==name )
	    {
	    ret_pi = (EvmsVolumeObject*)*Ptr_Ci;
	    }
	}
    y2milestone( "Volume %s has id %d", name.c_str(), 
                 ret_pi==NULL?0:ret_pi->id() );
    return( ret_pi );
    }

int EvmsAccess::createCo( const string& Container_Cv, 
                          unsigned long long PeSizeK_lv, bool NewMeta_bv, 
			  const list<string>& Devices_Cv )
    {
    int ret = 0;
    unsigned count = 0;
    Error_C.erase();
    CmdLine_C = "CreateCo " + Container_Cv + " PeSize " + decString(PeSizeK_lv);
    CmdLine_C += " Lvm2 " + decString(NewMeta_bv);
    CmdLine_C += " <";
    for( list<string>::const_iterator p=Devices_Cv.begin(); p!=Devices_Cv.end();
         p++ )
        {
	if( count>0 )
	    CmdLine_C += ",";
	CmdLine_C += *p;
        y2milestone( "Device %d %s", count++, p->c_str());
        }
    CmdLine_C += ">";
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    string name = Container_Cv;
    if( Container_Cv.find( "lvm/" )==0 || Container_Cv.find( "lvm2/" )==0)
	{
	name.erase( 0, name.find_first_of( "/" )+1 );
	}
    handle_array_t* input = NULL;
    if( ret==0 )
	{
	int count = Devices_Cv.size();
	input = (handle_array_t*)malloc( sizeof(handle_array_t)+
	                                 sizeof(object_handle_t)*count );
	if( input == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    input->count = count;
	    }
	}
    count = 0;
    list<string>::const_iterator p=Devices_Cv.begin(); 
    while( ret==0 && count<input->count )
	{
	string dev = undevDevice( *p );
	if( dev.find( "evms/" )==0 )
	    dev.erase( 0, 5 );
	if( Md::matchRegex( dev ))
	    dev = "md/" + dev;
	object_type_t ot = (object_type_t)(REGION|SEGMENT|DISK);
	y2mil( "dev:" << dev << " ot:" << ot  );
	ret = evms_get_object_handle_for_name( ot, (char *)dev.c_str(),
					       &input->handle[count] );
	y2milestone( "ret %d handle for %s is %d", ret, dev.c_str(), 
		     input->handle[count] );
	if( ret!=0 )
	    {
	    std::list<EvmsObject*>::const_iterator i=objects.begin();
	    while( i!=objects.end() && (*i)->type()!=EVMS_DISK && 
	           (*i)->name()!=dev )
		++i;
	    if( i!=objects.end() )
		{
		y2mil( "dev:" << dev << " id:" << (*i)->id()  );
		ret = evms_unassign( (*i)->id() );
		y2milestone( "ret %d evms_unassign id %d", ret, (*i)->id() );
		input->handle[count] = (*i)->id();
		}
	    if( ret!=0 )
		{
		i=objects.begin();
		while( i!=objects.end() && (*i)->type()!=EVMS_VOLUME && 
		       (*i)->name()!=dev )
		    ++i;
		if( i!=objects.end() )
		    {
		    ret = 0;
		    y2milestone( "volume with id %d", (*i)->id() );
		    input->handle[count] = (*i)->id();
		    }
		}
	    }
	if( ret )
	    {
	    y2milestone( "error: %s", evms_strerror(ret) );
	    Error_C = "could not find object " + *p;
	    ret = EVMS_INVALID_PHYSICAL_VOLUME;
	    }
	p++;
	count++;
	}
    count = 0;
    while( ret==0 && count<input->count )
	{
	object_handle_t use = findUsingVolume( input->handle[count] );
	if( use != 0 )
	    {
	    ret = evms_delete(use);
	    y2milestone( "evms_delete %d ret %d", use, ret ); 
	    if( ret )
		{
		Error_C = "could not delete using volume " + decString(use);
		y2milestone( "error: %s", evms_strerror(ret) );
		ret = EVMS_PHYSICAL_VOLUME_IN_USE;
		}
	    }
	count++;
	}
    plugin_handle_t lvm = 0;
    if( ret==0 )
	{
	lvm = getLvmPlugin( NewMeta_bv );
	if( lvm == 0 )
	    {
	    Error_C = "could not find lvm plugin";
	    ret = EVMS_PLUGIN_NOT_FOUND;
	    }
	}
    option_array_t* option = NULL;
    if( ret==0 )
	{
	count = 2;
	option = (option_array_t*) malloc( sizeof(option_array_t)+
	                                   count*sizeof(key_value_pair_t));
	if( option == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    option->count = count;
	    option->option[0].name = "name";
	    option->option[0].is_number_based = false;
	    option->option[0].type = EVMS_Type_String;
	    option->option[0].flags = 0;
	    option->option[0].value.s = (char*)name.c_str();
	    option->option[1].is_number_based = false;
	    option->option[1].flags = 0;
	    if( NewMeta_bv )
		{
		option->option[1].name = "extent_size";
		option->option[1].type = EVMS_Type_Unsigned_Int64;
		option->option[1].value.i64 = PeSizeK_lv*2;
		}
	    else
		{
		option->option[1].name = "pe_size";
		option->option[1].type = EVMS_Type_Unsigned_Int32;
		option->option[1].value.i32 = PeSizeK_lv*2;
		}
	    }
	}
    object_handle_t output;
    if( ret==0 )
	{
	ret = evms_create_container( lvm, input, option, &output );
	if( ret )
	    {
	    y2milestone( "evms_create_container ret %d", ret );
	    y2milestone( "error: %s", evms_strerror(ret) );
	    Error_C = "could not create container " + name;
	    ret = EVMS_CREATE_CONTAINER_FAILED;
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( option )
	free( option );
    if( input )
	free( input );
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::createLv( const string& LvName_Cv, const string& Container_Cv,
                          unsigned long long SizeK_lv, unsigned long Stripe_lv,
			  unsigned long long StripeSizeK_lv )
    {
    int ret = 0;
    y2milestone( "Name:%s Container:%s Size:%llu Stripe:%lu StripeSize:%llu", 
                 LvName_Cv.c_str(), Container_Cv.c_str(), SizeK_lv, Stripe_lv,
		 StripeSizeK_lv );
    Error_C.erase();
    CmdLine_C = "CreateLv " + LvName_Cv + " in " + Container_Cv + " Size:" + 
                decString(SizeK_lv) + "k";
    if( Stripe_lv>1 )
	{
	CmdLine_C += " Stripe:" + decString(Stripe_lv);
	if( StripeSizeK_lv>0 )
	    {
	    CmdLine_C += " StripeSize:" + decString(StripeSizeK_lv);
	    }
	}
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_UNSUPPORTED_CONTAINER_TYPE;
	}
    bool lvm2 = Container_Cv.find( "lvm2/" )==0;
    handle_array_t* reg = NULL;
    if( ret==0 )
	{
	reg = (handle_array_t*)malloc( sizeof(handle_array_t)+
	                               sizeof(object_handle_t) );
	if( reg == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    reg->count = 1;
	    reg->handle[0] = 0;
	    }
	}
    if( ret==0 )
	{
	string name = Container_Cv + "/Freespace";
	int ret = evms_get_object_handle_for_name( REGION, 
	                                           (char *)name.c_str(),
						   &reg->handle[0] );
	y2milestone( "ret %d handle for %s is %u", ret, name.c_str(),
	             reg->handle[0] );
	if( ret )
	    {
	    Error_C = "could not find object " + name;
	    y2milestone( "ret %s", evms_strerror(ret) );
	    ret = EVMS_CREATE_VOLUME_FREESPACE_NOT_FOUND;
	    }
	}
    plugin_handle_t lvm = 0;
    if( ret==0 )
	{
	lvm = getLvmPlugin( lvm2 );
	if( lvm == 0 )
	    {
	    Error_C = "could not find lvm plugin";
	    ret = EVMS_PLUGIN_NOT_FOUND;
	    }
	}
    option_array_t* option = NULL;
    if( ret==0 )
	{
	int count = 2;
	if( Stripe_lv>1 )
	    {
	    count++;
	    if( StripeSizeK_lv )
		count++;
	    }
	option = (option_array_t*) malloc( sizeof(option_array_t)+
	                                   count*sizeof(key_value_pair_t));
	if( option == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    option->count = count;
	    option->option[0].name = "name";
	    option->option[0].is_number_based = false;
	    option->option[0].type = EVMS_Type_String;
	    option->option[0].flags = 0;
	    option->option[0].value.s = (char*)LvName_Cv.c_str();
	    option->option[1].name = "size";
	    option->option[1].is_number_based = false;
	    option->option[1].flags = 0;
	    if( lvm2 )
		{
		option->option[1].type = EVMS_Type_Unsigned_Int64;
		option->option[1].value.i64 = SizeK_lv*2;
		}
	    else
		{
		option->option[1].type = EVMS_Type_Unsigned_Int32;
		option->option[1].value.i32 = SizeK_lv*2;
		}
	    if( Stripe_lv>1 )
		{
		option->option[2].name = "stripes";
		option->option[2].is_number_based = false;
		option->option[2].flags = 0;
		if( lvm2 )
		    {
		    option->option[2].type = EVMS_Type_Unsigned_Int64;
		    option->option[2].value.i64 = Stripe_lv;
		    }
		else
		    {
		    option->option[2].type = EVMS_Type_Unsigned_Int32;
		    option->option[2].value.i32 = Stripe_lv;
		    }
		if( StripeSizeK_lv )
		    {
		    option->option[3].name = "stripe_size";
		    option->option[3].is_number_based = false;
		    option->option[3].flags = 0;
		    if( lvm2 )
			{
			option->option[3].type = EVMS_Type_Unsigned_Int64;
			option->option[3].value.i64 = StripeSizeK_lv*2;
			}
		    else
			{
			option->option[3].type = EVMS_Type_Unsigned_Int32;
			option->option[3].value.i32 = StripeSizeK_lv*2;
			}
		    }
		}
	    }
	}
    handle_array_t* output = NULL;
    if( ret==0 )
	{
	ret = evms_create( lvm, reg, option, &output );
	if( ret )
	    {
	    y2milestone( "evms_create ret %d", ret );
	    y2milestone( "ret %s", evms_strerror(ret) );
	    Error_C = "could not create region " + LvName_Cv;
	    ret = EVMS_CREATE_VOLUME_FAILED;
	    }
	else
	    {
	    ret = evms_create_compatibility_volume( output->handle[0] );
	    if( ret )
		{
		y2milestone( "evms_create_compatibility_volume ret %d", ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not create compatibility volume " + LvName_Cv;
		ret = EVMS_CREATE_COMPAT_VOLUME_FAILED;
		}
	    }
	evms_free( output );
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( reg )
	free( reg );
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::createCompatVol( const string& Volume_Cv )
    {
    int ret = 0;
    y2milestone( "Name:%s", Volume_Cv.c_str() );
    Error_C.erase();
    CmdLine_C = "CreateCompatVol " + Volume_Cv;
    if( findVolume( Volume_Cv )==NULL )
	{
	string name = Volume_Cv.substr( 10 );
	const EvmsDataObject* Rg_p = findSegment( name );
	if( Rg_p != NULL )
	    {
	    ret = evms_create_compatibility_volume( Rg_p->id() );
	    if( ret )
		{
		y2milestone( "evms_create_compatibility_volume ret %d", ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not create compatibility volume " + Volume_Cv;
		ret = EVMS_CREATE_COMPAT_VOLUME_FAILED;
		}
	    }
	else
	    {
	    Error_C = "could not find segment " + name;
	    ret = EVMS_SEGMENT_NOT_FOUND;
	    }
	if( ret==0 && !endEvmsCommand() )
	    {
	    ret = EVMS_COMMIT_FAILED;
	    }
	}
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::deleteLv( const string& LvName_Cv, const string& Container_Cv )
    {
    int ret = 0;
    y2milestone( "Name:%s Container:%s", LvName_Cv.c_str(), 
                 Container_Cv.c_str() );
    Error_C.erase();
    CmdLine_C = "RemoveLv " + LvName_Cv + " in " + Container_Cv;
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_UNSUPPORTED_CONTAINER_TYPE;
	}
    handle_array_t* reg = NULL;
    if( ret==0 )
	{
	reg = (handle_array_t*)malloc( sizeof(handle_array_t)+
	                               sizeof(object_handle_t) );
	if( reg == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    reg->count = 1;
	    reg->handle[0] = 0;
	    }
	}
    if( ret==0 )
	{
	string name = Container_Cv + "/" + LvName_Cv;
	int ret = evms_get_object_handle_for_name( REGION, 
	                                           (char *)name.c_str(),
						   &reg->handle[0] );
	y2milestone( "ret %d handle for %s is %u", ret, name.c_str(),
	             reg->handle[0] );
	if( ret )
	    {
	    Error_C = "could not find object " + name;
	    y2milestone( "ret %s", evms_strerror(ret) );
	    ret = EVMS_REMOVE_VOLUME_NOT_FOUND;
	    }
	}
    if( ret==0 )
	{
	object_handle_t use = findUsingVolume( reg->handle[0] );
	if( use != 0 )
	    {
	    ret = evms_delete(use);
	    y2milestone( "evms_delete %d ret %d", use, ret ); 
	    if( ret )
		{
		Error_C = "could not delete using volume " + decString(use);
		y2milestone( "error: %s", evms_strerror(ret) );
		ret = EVMS_PHYSICAL_VOLUME_IN_USE;
		}
	    }
	if( ret==0 )
	    {
	    ret = evms_delete( reg->handle[0] );
	    if( ret )
		{
		y2milestone( "evms_delete ret %d", ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not delete region " + LvName_Cv;
		ret = EVMS_REMOVE_REGION_FAILED;
		}
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( reg )
	free( reg );
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::extendCo( const string& Container_Cv, const string& PvName_Cv )
    {
    int ret = 0;
    y2milestone( "Container:%s PvName:%s", Container_Cv.c_str(), 
                 PvName_Cv.c_str() );
    Error_C.erase();
    CmdLine_C = "ExtendCo " + Container_Cv + " by " + PvName_Cv;
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_UNSUPPORTED_CONTAINER_TYPE;
	}
    object_handle_t region = 0;
    if( ret==0 )
	{
	string dev = undevDevice( PvName_Cv );
	object_type_t ot = (object_type_t)(REGION|SEGMENT);
	int ret = evms_get_object_handle_for_name( ot, 
	                                           (char *)dev.c_str(),
						   &region );
	y2milestone( "ret %d handle for %s is %d", ret, dev.c_str(), 
	             region );
	if( ret )
	    {
	    y2milestone( "error: %s", evms_strerror(ret) );
	    Error_C = "could not find object " + PvName_Cv;
	    ret = EVMS_INVALID_PHYSICAL_VOLUME;
	    }
	}
    if( ret==0 )
	{
	object_handle_t use = findUsingVolume( region );
	if( use != 0 )
	    {
	    ret = evms_delete(use);
	    y2milestone( "evms_delete %d ret %d", use, ret ); 
	    if( ret )
		{
		Error_C = "could not delete using volume " + decString(use);
		y2milestone( "error: %s", evms_strerror(ret) );
		ret = EVMS_PHYSICAL_VOLUME_IN_USE;
		}
	    }
	}
    const EvmsContainerObject* Co_p = NULL;
    if( ret==0 )
	{
	Co_p = findContainer( Container_Cv );
	if( Co_p == NULL )
	    {
	    Error_C = "could not find container " + Container_Cv;
	    ret = EVMS_CONTAINER_NOT_FOUND;
	    }
	}
    handle_array_t* reg = NULL;
    if( ret==0 )
	{
	reg = (handle_array_t*)malloc( sizeof(handle_array_t)+
	                               sizeof(object_handle_t) );
	if( reg == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    reg->count = 1;
	    reg->handle[0] = region;
	    }
	}
    if( ret==0 && Co_p )
	{
	ret = evms_expand( Co_p->id(), reg, NULL );
	if( ret )
	    {
	    Error_C = "could not expand container " + Container_Cv + " by " + 
		      PvName_Cv;
	    y2milestone( "ret %s", evms_strerror(ret) );
	    ret = EVMS_CONTAINER_EXPAND_FAILED;
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( reg )
	free( reg );
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::shrinkCo( const string& Container_Cv, const string& PvName_Cv )
    {
    int ret = 0;
    y2milestone( "Container:%s PvName:%s", Container_Cv.c_str(), 
                 PvName_Cv.c_str() );
    Error_C.erase();
    CmdLine_C = "ShrinkCo " + Container_Cv + " by " + PvName_Cv;
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_UNSUPPORTED_CONTAINER_TYPE;
	}
    object_handle_t region = 0;
    if( ret==0 )
	{
	string dev = undevDevice( PvName_Cv );
	object_type_t ot = (object_type_t)(REGION|SEGMENT);
	int ret = evms_get_object_handle_for_name( ot, 
	                                           (char *)dev.c_str(),
						   &region );
	y2milestone( "ret %d handle for %s is %d", ret, dev.c_str(), 
	             region );
	if( ret )
	    {
	    y2milestone( "error: %s", evms_strerror(ret) );
	    Error_C = "could not find object " + PvName_Cv;
	    ret = EVMS_CONTAINER_SHRINK_INVALID_SEGMENT;
	    }
	}
    const EvmsContainerObject* Co_p = NULL;
    if( ret==0 )
	{
	Co_p = findContainer( Container_Cv );
	if( Co_p == NULL )
	    {
	    Error_C = "could not find container " + Container_Cv;
	    ret = EVMS_CONTAINER_NOT_FOUND;
	    }
	}
    handle_array_t* reg = NULL;
    if( ret==0 )
	{
	reg = (handle_array_t*)malloc( sizeof(handle_array_t)+
	                               sizeof(object_handle_t) );
	if( reg == NULL )
	    {
	    Error_C = "out of memory";
	    ret = EVMS_MALLOC_FAILED;
	    }
	else
	    {
	    reg->count = 1;
	    reg->handle[0] = region;
	    }
	}
    if( ret==0 && Co_p )
	{
	ret = evms_shrink( Co_p->id(), reg, NULL );
	if( ret )
	    {
	    Error_C = "could not transfer " + PvName_Cv + " out of container " +
	              Container_Cv;
	    y2milestone( "ret %s", evms_strerror(ret) );
	    ret = EVMS_CONTAINER_SHRINK_FAILED;
	    }
	else
	    {
	    ret = evms_create_compatibility_volume( region );
	    if( ret )
		{
		y2milestone( "evms_create_compatibility_volume ret %d", 
			     ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not create compatibility volume " + 
			  PvName_Cv;
		ret = EVMS_CREATE_COMPAT_VOLUME_FAILED;
		}
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( reg )
	free( reg );
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::deleteCo( const string& Container_Cv )
    {
    int ret = 0;
    y2milestone( "Container:%s", Container_Cv.c_str() );
    Error_C.erase();
    CmdLine_C = "DeleteCo " + Container_Cv;
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_UNSUPPORTED_CONTAINER_TYPE;
	}
    const EvmsContainerObject* Co_p = NULL;
    if( ret==0 )
	{
	Co_p = findContainer( Container_Cv );
	if( Co_p == NULL )
	    {
	    Error_C = "could not find container " + Container_Cv;
	    ret = EVMS_CONTAINER_NOT_FOUND;
	    }
	else
	    {
	    y2milestone( "handle for %s is %u", 
	                 Container_Cv.c_str(), Co_p->id() );
	    for( list<EvmsContainerObject::peinfo>::const_iterator p=Co_p->consumes().begin(); 
	         p!=Co_p->consumes().end(); p++ )
		{
		y2milestone( "consumes %d", p->obj->id() );
		}
	    }
	}
    if( ret==0 )
	{
	ret = evms_delete( Co_p->id() );
	if( ret )
	    {
	    y2milestone( "evms_delete ret %d", ret );
	    y2milestone( "ret %s", evms_strerror(ret) );
	    Error_C = "could not delete container " + Container_Cv;
	    ret = EVMS_CONTAINER_REMOVE_FAILED;
	    }
	else
	    {
	    for( list<EvmsContainerObject::peinfo>::const_iterator p=Co_p->consumes().begin(); 
	         p!=Co_p->consumes().end(); p++ )
		{
		ret = evms_create_compatibility_volume( p->obj->id() );
		if( ret )
		    {
		    y2milestone( "evms_create_compatibility_volume ret %d", 
		                 ret );
		    y2milestone( "ret %s", evms_strerror(ret) );
		    Error_C = "could not create compatibility volume " + 
		              p->obj->name();
		    ret = EVMS_CREATE_COMPAT_VOLUME_FAILED;
		    }
		}
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EvmsAccess::changeLvSize( const string& Name_Cv, const string& Container_Cv,
			      unsigned long long SizeK_lv )
    {
    int ret = 0;
    Error_C.erase();
    CmdLine_C = "ChangeLvSize of " + Name_Cv + " in " + Container_Cv +
                " to " + decString(SizeK_lv) + "k";
    y2milestone( "CmdLine_C %s", CmdLine_C.c_str());
    if( Container_Cv.find( "lvm/" )!=0 && Container_Cv.find( "lvm2/" )!=0 )
	{
	Error_C = "unknown container type: " + Container_Cv;
	ret = EVMS_RESIZE_CONTAINER_NOT_FOUND;
	}
    bool lvm2 = Container_Cv.find( "lvm2/" )==0;
    const EvmsDataObject* Rg_p = NULL;
    if( ret==0 )
	{
	Rg_p = findRegion( Container_Cv, Name_Cv );
	if( Rg_p == NULL )
	    {
	    Error_C = "could not find volume " + Name_Cv + " in " + Container_Cv;
	    ret = EVMS_RESIZE_VOLUME_NOT_FOUND;
	    }
	else
	    {
	    y2milestone( "handle for %s in %s is %u", Name_Cv.c_str(),
	                 Container_Cv.c_str(), Rg_p->id() );
	    }
	}
    if( ret==0 )
	{
	y2milestone( "old size:%llu new size:%llu", Rg_p->sizeK(), SizeK_lv );
	option_array_t option;
	option.count = 1;
	if( SizeK_lv > Rg_p->sizeK() )
	    {
	    option.option[0].is_number_based = false;
	    option.option[0].flags = 0;
	    if( !lvm2 )
		{
		option.option[0].name = "add_size";
		option.option[0].type = EVMS_Type_Unsigned_Int32;
		option.option[0].value.i32 = (SizeK_lv-Rg_p->sizeK())*2;
		}
	    else
		{
		option.option[0].name = "size";
		option.option[0].type = EVMS_Type_Unsigned_Int64;
		option.option[0].value.i64 = (SizeK_lv-Rg_p->sizeK())*2;
		}
	    ret = evms_expand( Rg_p->id(), NULL, &option );
	    if( ret )
		{
		y2milestone( "evms_expand ret %d", ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not expand volume " + Name_Cv + " to " +
		          decString(SizeK_lv) + "k";
		ret = EVMS_RESIZE_EXPAND_FAILED;
		}
	    }
	else if( SizeK_lv < Rg_p->sizeK() )
	    {
	    option.option[0].flags = 0;
	    option.option[0].is_number_based = false;
	    if( !lvm2 )
		{
		option.option[0].name = "remove_size";
		option.option[0].type = EVMS_Type_Unsigned_Int32;
		option.option[0].value.i32 = (Rg_p->sizeK()-SizeK_lv)*2;
		}
	    else
		{
		option.option[0].name = "size";
		option.option[0].type = EVMS_Type_Unsigned_Int64;
		option.option[0].value.i64 = (Rg_p->sizeK()-SizeK_lv)*2;
		}
	    ret = evms_shrink( Rg_p->id(), NULL, &option );
	    if( ret )
		{
		y2milestone( "evms_shrink ret %d", ret );
		y2milestone( "ret %s", evms_strerror(ret) );
		Error_C = "could not shrink volume " + Name_Cv + " to " +
		          decString(SizeK_lv) + "k";
		ret = EVMS_RESIZE_SHRINK_FAILED;
		}
	    }
	}
    if( ret==0 && !endEvmsCommand() )
	{
	ret = EVMS_COMMIT_FAILED;
	}
    if( Error_C.size()>0 )
	{
	y2milestone( "Error: %s", Error_C.c_str() );
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

bool EvmsAccess::endEvmsCommand()
    {
    int ret = evms_commit_changes();
    if( ret )
	{
	y2milestone( "evms_commit_changes ret %d", ret );
	Error_C = "could not commit changes";
	}
    rereadAllObjects();
    return( ret==0 );
    }

void EvmsAccess::output( ostream& str ) const
    {
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	switch( (*Ptr_Ci)->type() )
	    {
	    case EVMS_DISK:
	    case EVMS_SEGMENT:
	    case EVMS_REGION:
	    case EVMS_OBJ:
		str << *(EvmsDataObject*)*Ptr_Ci;
		break;
	    case EVMS_CONTAINER:
		str << *(EvmsContainerObject*)*Ptr_Ci;
		break;
	    case EVMS_VOLUME:
		str << *(EvmsVolumeObject*)*Ptr_Ci;
		break;
	    default:
		str << **Ptr_Ci;
		break;
	    }
	}
    }

void EvmsAccess::listLibstorage( std::ostream &str ) const
    {
    for( list<EvmsObject*>::const_iterator Ptr_Ci = objects.begin(); 
         Ptr_Ci != objects.end(); Ptr_Ci++ )
	{
	switch( (*Ptr_Ci)->type() )
	    {
	    case EVMS_DISK:
	    case EVMS_SEGMENT:
	    case EVMS_REGION:
	    case EVMS_OBJ:
		str << *(EvmsDataObject*)*Ptr_Ci;
		break;
	    case EVMS_CONTAINER:
		{
		EvmsContainerObject* co = (EvmsContainerObject*)*Ptr_Ci;
		if( co->isLvm() )
		    {
		    str << *co;
		    }
		}
		break;
	    case EVMS_VOLUME:
		str << *(EvmsVolumeObject*)*Ptr_Ci;
		break;
	    default:
		str << **Ptr_Ci;
		break;
	    }
	}
    }


namespace storage
{

ostream& operator<<( ostream &str, const EvmsAccess& obj )
    {
    obj.output( str );
    return( str );
    }

}
