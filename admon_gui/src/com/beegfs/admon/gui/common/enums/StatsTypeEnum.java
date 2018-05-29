package com.beegfs.admon.gui.common.enums;

import com.beegfs.admon.gui.program.Main;


public enum StatsTypeEnum
{
   STATS_NONE("None type", "none"),
   STATS_CLIENT_METADATA(Main.getLocal().getString("Client stats metadata"), "ClientStatsMeta"),
   STATS_CLIENT_STORAGE(Main.getLocal().getString("Client stats storage"), "ClientStatsStorage"),
   STATS_USER_METADATA(Main.getLocal().getString("User stats metadata"), "UserStatsMetadata"),
   STATS_USER_STORAGE(Main.getLocal().getString("User stats storage"), "UserStatsStorage");

   private final String description;
   private final String type;

   StatsTypeEnum(String description, String type)
   {
      this.description = description;
      this.type = type;
   }

   public String description()
   {
      return description;
   }

   public String type()
   {
      return type;
   }
}
