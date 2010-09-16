
#include <stdlib.h>
#include <iostream>

#include "common.h"


namespace storage
{
    using namespace std;


    void
    print_commitinfos(const StorageInterface* s)
    {
	list<CommitInfo> infos;
	s->getCommitInfos(infos);
	for (list<CommitInfo>::const_iterator it = infos.begin(); it != infos.end(); ++it)
	    cout << it->text << endl;
	cout << endl;
    }

}
