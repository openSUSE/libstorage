#!/usr/bin/ruby

require 'storage'


print Storage.numSuffixes, "\n"

print Storage.getSuffix(3, false), "\n"

print Storage.byteToHumanString(123456, false, 2, false), "\n"
