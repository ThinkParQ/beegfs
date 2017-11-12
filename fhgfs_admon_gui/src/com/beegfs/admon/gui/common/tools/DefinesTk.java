package com.beegfs.admon.gui.common.tools;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class DefinesTk
{
   public static final String BEEGFS_ADMON_GUI_NAMESPACE = "com.beegfs.admon.gui";
   public static final int CLIENT_STATS_DEFAULT_COLUMN_WIDTH = 50;
   public static final int CLIENT_STATS_COLUMN_WIDTH_IP = 100;
   public static final String DEFAULT_ENCODING_UTF8 = "UTF-8";
   public static final int DEFAULT_STRING_LENGTH = 128;
   public static final int STRIPEPATTERN_MIN_CHUNKSIZE = 1024 * 64;

   public static final List<String> CHART_TIME_INTERVAL = Collections.unmodifiableList(
      Arrays.asList(
      "10 minutes (1 sec samples)",
      "20 minutes (1 sec samples)",
      "30 minutes (1 sec samples)",
      "40 minutes (1 sec samples)",
      "50 minutes (1 sec samples)",
      "60 minutes (1 sec samples)",
      "2 hours (1 sec samples)",
      "4 hours (1 sec samples)",
      "6 hours (1 sec samples)",
      "12 hours (1 sec samples)",
      "24 hours (1 sec samples)",
      "2 days (1 hour samples)",
      "3 days (1 hour samples)",
      "4 days (1 hour samples)",
      "5 days (1 hour samples)"
   ));

   private DefinesTk()
   {
   }
}
