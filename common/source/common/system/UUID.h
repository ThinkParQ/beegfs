#pragma once

#include <common/storage/StorageErrors.h>
#include <common/system/System.h>

#include <blkid/blkid.h>

// This is a library that is meant to be included where access to system UUIDs is needed. It is
// header-only so it doesn't get built into libbeegfs-common which would inject a dependency on
// libblkid into everything that links common.
namespace UUID {

/*
 * Public method to get the file system UUID for given mountpoint.
 *
 * Used to get a file system UUID that can be compared to a configured UUID to make sure services
 * start only after their storage properly mounted. Will throw an InvalidConfigException if any
 * error is encountered.
 *
 * @param mountpoint the mountpoint to get the file system UUID for
 * @return string the UUID of the filesystem mounted at mountpoint
 */
std::string getFsUUID(std::string mountpoint)
{
   std::string device_path = System::getDevicePathFromMountpoint(mountpoint);

   // Lookup the fs UUID
   std::unique_ptr<blkid_struct_probe, void(*)(blkid_struct_probe*)>
         probe(blkid_new_probe_from_filename(device_path.data()), blkid_free_probe);

   if (!probe) {
      throw InvalidConfigException("Failed to open device for probing: " + device_path + " (check BeeGFS is running with root or sufficient privileges)");
   }

   if (blkid_probe_enable_superblocks(probe.get(), 1) < 0) {
      throw InvalidConfigException("Failed to enable superblock probing");
   }

   if (blkid_do_fullprobe(probe.get()) < 0) {
      throw InvalidConfigException("Failed to probe device");
   }

   const char* uuid = nullptr; // gets released automatically
   if (blkid_probe_lookup_value(probe.get(), "UUID", &uuid, nullptr) < 0) {
      throw InvalidConfigException("Failed to lookup file system UUID");
   }

   std::string uuid_str(uuid);
   return uuid_str;
}

/*
 * Public method to get a partition UUID that can be used to uniquely identify the machine
 *
 * This function will iterate over all partitions in the blkid cache, which is accessible as an
 * unprivileged user, and return the first suitable UUID. The selection algorithm for a suitable
 * UUID is:
 *  1. Must be either the "UUID" or "PARTUUID" of a partition. "UUID" is preferred over "PARTUUID"
 *  2. Partitions are processed in the order they appear in the cache. The order should be stable at
 *     least as long as the machine isn't rebooted, typically even across reboots.
 *  3. Only UUIDs that are 36 characters long (like the standard UUID4 format) are considered, with
 *     the sole exception of the fallback (see 5.)
 *  4. If the device mounted at the given mountpoint has either a "UUID" or "PARTUUID" that is 36
 *     characters long, this function will always return that ID.
 *  5. If no suitable UUID with a length of 36 characters has been found after iterating all
 *     partitions, the first non-empty "UUID" that was found will be returned as a fallback.
 *
 * @param mountpoint mountpoint of preferred partition
 * @return pair<FhgfsOpsErr, string> containing either FhgfsOpsErr_SUCCESS and the uuid or an error
 * code and the error reason
 */
std::pair<FhgfsOpsErr, std::string> getPartUUID(std::string mountpoint) {
   std::string out, fallback;

   std::string device_path = System::getDevicePathFromMountpoint(mountpoint);

   blkid_cache cache;
   int err = blkid_get_cache(&cache, NULL);
   if (err)
      return std::pair(FhgfsOpsErr_INTERNAL, "unable to read blkid cache to get partition uuid");
   blkid_probe_all(cache);

   auto diter = blkid_dev_iterate_begin(cache);
   blkid_dev dev;
   while(!blkid_dev_next(diter, &dev)) {
      auto devname = blkid_dev_devname(dev);
      if (devname == nullptr)
         continue;

      std::string uuid;
      auto uuid_raw = blkid_get_tag_value(cache, "UUID", devname);
      if (uuid_raw != nullptr)
         uuid = std::string(uuid_raw);

      std::string part_uuid;
      auto part_uuid_raw = blkid_get_tag_value(cache, "PARTUUID", devname);
      if (part_uuid_raw != nullptr)
         part_uuid = std::string(part_uuid_raw);

      if(!uuid.empty()) {
         out = uuid;
         if(devname == device_path || uuid.length() == 36)
            break;
         if(fallback.empty())
            fallback = uuid;
      }
      if(!part_uuid.empty()) {
         out = part_uuid;
         if(devname == device_path || part_uuid.length() == 36)
            break;
      }
   }
   blkid_dev_iterate_end(diter);

   if(out.empty() || out.length() != 36) {
      if(!fallback.empty())
         out = fallback;
      else
         return std::pair(FhgfsOpsErr_INTERNAL, "no usable partition IDs found in blkid cache");
      }

   return std::pair(FhgfsOpsErr_SUCCESS, out);
}

/*
 * Read the machine's dmi UUID from sysfs or fall back to the first suitable partition UUID
 *
 * NOTE: The dmi UUID is probably the best (not user changeable, hardware bound) machine ID we can
 * get, but the file containing it is only readable as root. The meta and storage services typically
 * run as root, but they can technically run as unprivileged users. If the machine ID can not be
 * read from dmi, this function will return a suitable partition UUID via getPartUUID().
 *
 * @return string containing the UUID or empty string on error
 */
 std::string getMachineUUID() {
   const char* logContext = "System (get machine UUID)";

   char buf[37];
   bool failed = false;

   // This path should be canonical and stable across Linux distributions
   std::ifstream in("/sys/class/dmi/id/product_uuid");
   if(!in.is_open())
      failed = true;
   in.getline(buf, 37);
   in.close();

   std::string machineUUID = std::string(buf);
   if(machineUUID.length() != 36)
      failed = true;

   if(!failed)
      return machineUUID;

   // Fall back to partition UUID
   auto [err, res] = getPartUUID("/");
   if(err != FhgfsOpsErr_SUCCESS) {
      LogContext(logContext).log(Log_WARNING, "Error while reading file system UUID: " + res);
      return "";
   }
   return res;
}

} // namespace UUID
