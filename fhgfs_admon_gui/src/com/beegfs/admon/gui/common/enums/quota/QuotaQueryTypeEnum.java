package com.beegfs.admon.gui.common.enums.quota;


public enum QuotaQueryTypeEnum
{
   GET_QUOTA_ID_LIST("List of IDs", 0),
   SET_QUOTA_ID_LIST("List of IDs", 0),
   SET_QUOTA_RANGE("Range of IDs", 1);

   private static final int numberOfGetValues = 1;
   private static final int numberOfSetValues = values().length - numberOfGetValues;

   private final String comboBoxValue;
   private final int indexComboBox;

   QuotaQueryTypeEnum(String comboBoxValue, int indexComboBox)
   {
      this.comboBoxValue = comboBoxValue;
      this.indexComboBox = indexComboBox;
   }

   @Override
   public String toString()
   {
      return comboBoxValue;
   }

   public int getIndex()
   {
      return indexComboBox;
   }

   static public String[] getValuesForComboBoxSetQuota()
   {
      QuotaQueryTypeEnum[] types = QuotaQueryTypeEnum.values();
      String[] retVal = new String[numberOfSetValues];

      for (int index = numberOfGetValues; index < types.length; index ++)
      {
         retVal[types[index].getIndex()] = types[index].toString();
      }

      return retVal;
   }

   static public String[] getValuesForComboBoxGetQuota()
   {
      QuotaQueryTypeEnum[] types = QuotaQueryTypeEnum.values();
      String[] retVal = new String[numberOfGetValues];

      for (int index = 0; index < numberOfGetValues; index ++)
      {
         retVal[types[index].getIndex()] = types[index].toString();
      }

      return retVal;
   }
}
