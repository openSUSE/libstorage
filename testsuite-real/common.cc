
#include <stdlib.h>
#include <iostream>

#include "common.h"


namespace storage
{
    using namespace std;


    void
    print_fstab()
    {
	ifstream fstab("/etc/fstab");

	cout << "begin of fstab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of fstab" << endl;
    }


    void
    print_crypttab()
    {
	ifstream fstab("/etc/crypttab");

	cout << "begin of crypttab" << endl;
	string line;
	while (getline(fstab, line))
	    cout << line << endl;
	cout << "end of crypttab" << endl;
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
