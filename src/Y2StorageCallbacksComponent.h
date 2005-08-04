
#include <y2/Y2Namespace.h>
#include <y2/Y2Component.h>

class Y2StorageCallbacksComponent : public Y2Component
{
public:
    virtual Y2Namespace *import (const char* name);

    virtual string name () const { return "StorageCallbacks"; }

    static Y2StorageCallbacksComponent* instance();

private:
    static Y2StorageCallbacksComponent* m_instance;
};

