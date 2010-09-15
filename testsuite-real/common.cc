
#include <stdlib.h>
#include <iostream>

#include "common.h"


namespace storage
{
    using namespace std;


    void
    check_failure(const char* str, const char* file, int line, const char* func)
    {
	cerr << "failure in \"" << func << "\" line " << line << endl;
	cerr << "check \"" << str << "\"" << endl;

	exit(EXIT_FAILURE);
    }


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
