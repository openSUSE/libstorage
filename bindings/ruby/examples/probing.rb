#!/usr/bin/ruby

require 'storage'


env = Storage::Environment.new(true)

c = Storage::createStorageInterface(env)

containers = Storage::Dequecontainerinfo.new()
c.getContainers(containers)

containers.each do |container|
    puts container.device
end

Storage::destroyStorageInterface(c)
