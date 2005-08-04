
#define y2log_component libstorage

#include <y2util/y2log.h>
#include <y2/Y2Namespace.h>
#include <y2/Y2Component.h>
#include <y2/Y2ComponentCreator.h>

#include "Y2StorageCallbacksComponent.h"

#include "StorageCallbacks.h"

Y2Namespace *Y2StorageCallbacksComponent::import (const char* name)
{
    // FIXME: for internal components, we should track changes in symbol numbering
    if ( strcmp (name, "StorageCallbacks") == 0)
    {
	return StorageCallbacks::instance ();
    }
	
    return NULL;
}

Y2StorageCallbacksComponent* Y2StorageCallbacksComponent::m_instance = NULL;

Y2StorageCallbacksComponent* Y2StorageCallbacksComponent::instance ()
{
    if (m_instance == NULL)
    {
        m_instance = new Y2StorageCallbacksComponent ();
    }

    return m_instance;
}

