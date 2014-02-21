
#include <iostream>

#include "common.h"

#include "storage/Region.h"


using namespace std;
using namespace storage;


int
main()
{
    cout.setf(std::ios::boolalpha);

    setup_logger();

    setup_system("thalassa");

    StorageInterface* s = createStorageInterface(TestEnvironment());

    string disk = "/dev/sdc";

    s->destroyPartitionTable(disk, "msdos");

    long int S = 8 * 1000000;

    string name;

    cout << s->createPartitionKb(disk, PRIMARY, S, S, name) << endl;
    cout << name << endl;

    cout << s->createPartitionKb(disk, EXTENDED, 3*S, 3*S, name) << endl;
    cout << name << endl;

    cout << s->createPartitionKb(disk, LOGICAL, 4*S, S, name) << endl;
    cout << name << endl;

    print_partitions(s, disk);

    list<PartitionSlotInfo> slots;
    cout << s->getUnusedPartitionSlots(disk, slots) << endl;

    slots.sort([](const PartitionSlotInfo& rhs, const PartitionSlotInfo& lhs) {
	return rhs.cylRegion.start < lhs.cylRegion.start;
    });

    for (const PartitionSlotInfo& slot : slots)
    {
	cout << Region(slot.cylRegion) << "  " << slot.nr << " " << slot.device << "  "
	     << slot.primarySlot << " " << slot.primaryPossible << "  "
	     << slot.extendedSlot << " " << slot.extendedPossible << "  "
	     << slot.logicalSlot << " " << slot.logicalPossible << endl;
    }

    delete s;
}
