#ifndef EVMS_ACCESS_H
#define EVMS_ACCESS_H

extern "C"
{
#define bool boolean
#include <appAPI.h>
#undef bool
#undef min
#undef max
#undef _
}

#include <list>
#include <ostream>

using std::string;

typedef enum { EVMS_UNKNOWN, EVMS_DISK, EVMS_SEGMENT, EVMS_REGION, EVMS_PLUGIN,
               EVMS_CONTAINER, EVMS_VOLUME, EVMS_OBJ } ObjType;

class EvmsAccess;

class EvmsObject
    {
    public:
	EvmsObject() { init(); };
	EvmsObject( object_handle_t id );
	virtual ~EvmsObject();

	ObjType type() const { return typ; };
	const string& name() const { return nam; };
	const object_handle_t id() const { return idt; };

	void disownPtr() { own_ptr = false; };
	void output( std::ostream& Stream ) const;
	unsigned long long sizeK() const { return size; };
	bool isDataType() const;
	bool isData() const { return is_data; };
	bool isFreespace() const { return is_freespace; };
	virtual void addRelation( EvmsAccess* Acc ) {};

    protected:
	void init();

	bool own_ptr;
	bool is_data;
	bool is_freespace;
	unsigned long long size;
	ObjType typ;
	object_handle_t idt;
	string nam;
	handle_object_info_t *info_p;
    };

class EvmsDataObject : public EvmsObject
    {
    public:
	EvmsDataObject( EvmsObject *const obj );
	EvmsDataObject( object_handle_t id );
	const EvmsObject *const consumedBy() const { return consumed; };
	const EvmsObject *const volume() const { return vol; };
	void output( std::ostream& Stream ) const;
	virtual void addRelation( EvmsAccess* Acc );

    protected:
	void init();
	storage_object_info_t* getInfop();

	EvmsObject * consumed;
	EvmsObject * vol;
    };

class EvmsContainerObject : public EvmsObject
    {
    public:
	struct peinfo
	    {
	    EvmsObject * obj;
	    unsigned long long size;
	    unsigned long long free;
	    string uuid;
	    };

	EvmsContainerObject( EvmsObject *const obj );
	EvmsContainerObject( object_handle_t id );
	unsigned long long freeK() const { return free; };
	unsigned long long peSize() const { return pe_size; };
	const std::list<peinfo>& consumes() const { return cons; };
	const std::list<EvmsObject *>& creates() const { return creat; };
	const string& typeName() const { return ctype; };
	void output( std::ostream& Stream ) const;
	virtual void addRelation( EvmsAccess* Acc );

    protected:
	void init();
	storage_container_info_t* getInfop();

	unsigned long long free;
	unsigned long long pe_size;
	string uuid;
	std::list<peinfo> cons;
	std::list<EvmsObject *> creat;
	string ctype;
	bool lvm1;
    };

class EvmsVolumeObject : public EvmsObject
    {
    public:
	EvmsVolumeObject( EvmsObject *const obj );
	EvmsVolumeObject( object_handle_t id );
	const EvmsObject * consumedBy() const { return consumed; };
	EvmsObject * consumes() const { return cons; };
	const EvmsObject * assVol() const { return assc; };
	bool native() const { return nat; };
	const string& device() const { return dev; };
	void output( std::ostream& Stream ) const;
	void setConsumedBy( EvmsObject* Obj );
	virtual void addRelation( EvmsAccess* Acc );

    protected:
	void init();
	logical_volume_info_s* getInfop();

	bool nat;
	EvmsObject * consumed;
	EvmsObject * cons;
	EvmsObject * assc;
	string dev;
    };

class EvmsAccess
    {
    public:
	EvmsObject *const addObject( object_handle_t id );
	EvmsObject *const find( object_handle_t id );
	EvmsAccess();
	~EvmsAccess();
	void output( std::ostream &Stream ) const;
	void listVolumes( std::list<const EvmsVolumeObject*>& l ) const;
	void listContainer(std::list<const EvmsContainerObject*>& l ) const;
	const string& getErrorText() {return Error_C;};
	const string& getCmdLine() {return CmdLine_C;};
	int deleteCo( const string& Container_Cv );
	int extendCo( const string& Container_Cv, const string& PvName_Cv );
	int shrinkCo( const string& Container_Cv, const string& PvName_Cv );
	int createCo( const string& Container_Cv, unsigned long long PeSizeK_lv,
		       bool NewMeta_bv, const std::list<string>& Devices_Cv );
	int createLv( const string& LvName_Cv, const string& Container_Cv,
		      unsigned long long SizeK_lv, unsigned long Stripe_lv,
		      unsigned long long StripeSizeK_lv );
	int changeLvSize( const string& LvName_Cv, const string& Container_Cv,
	                  unsigned long long SizeK_lv );
	int deleteLv( const string& LvName_Cv, const string& Container_Cv );
	int createCompatVol( const string& Volume_Cv );

    protected:
	void addObjectRelations();
	void rereadAllObjects();
	plugin_handle_t getLvmPlugin( bool lvm2=false );
	object_handle_t findUsingVolume( object_handle_t id );
	const EvmsContainerObject* findContainer( const string& name );
	const EvmsDataObject* findRegion( const string& container, 
	                                  const string& name );
	const EvmsDataObject* findSegment( const string& name );
	const EvmsVolumeObject* findVolume( const string& name );
	bool endEvmsCommand();
	static int pluginFilterFunction( const char* plugin );

	std::list<EvmsObject*> objects;
	bool EvmsOpen_b;
	string Error_C;
	string CmdLine_C;
    };

extern std::ostream& operator<<( std::ostream &Stream, const ObjType Obj );
extern std::ostream& operator<<( std::ostream &Stream, const EvmsAccess& Obj );
extern std::ostream& operator<<( std::ostream &Stream, const EvmsObject& Obj );
extern std::ostream& operator<<( std::ostream &Stream, const EvmsDataObject& Obj );
extern std::ostream& operator<<( std::ostream &Stream, const EvmsContainerObject& Obj );
extern std::ostream& operator<<( std::ostream &Stream, const EvmsVolumeObject& Obj );

#endif
