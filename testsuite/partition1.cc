
#include <iostream>
#include <iterator>

#include <y2storage/StorageInterface.h>

using namespace std;
using namespace storage;


int
main ()
{
    setenv ("YAST2_STORAGE_TDIR", ".", 1);

    StorageInterface* s = createStorageInterface (true, true, true);

    string disk = "/dev/hda";


    list<PartitionInfo> partitioninfos;
    s->getPartitions (disk, partitioninfos);
    for (list<PartitionInfo>::iterator i = partitioninfos.begin ();
	 i != partitioninfos.end(); i++)
	cout << i->name << '\n';
    cout << '\n';


    s->destroyPartitionTable (disk, s->defaultDiskLabel ());


    long int S = 100000;
    string name;

    cout << s->createPartitionKb (disk, PRIMARY,  0*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, PRIMARY,  1*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, PRIMARY,  2*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, EXTENDED, 3*S, 4*S, name) << '\n';
    cout << s->createPartitionKb (disk, LOGICAL,  3*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, LOGICAL,  4*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, LOGICAL,  5*S, S, name) << '\n';
    cout << s->createPartitionKb (disk, LOGICAL,  6*S, S, name) << '\n';


    s->getPartitions (disk, partitioninfos);
    for (list<PartitionInfo>::iterator i = partitioninfos.begin ();
	 i != partitioninfos.end(); i++)
	cout << i->name << ' ' << i->cylStart << ' ' << i->cylSize << '\n';
    cout << '\n';


    delete s;
}
