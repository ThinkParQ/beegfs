package com.beegfs.admon.gui.common.enums.quota;


public enum QuotaIDTypeEnum
{
   USER("User quota", 0),
   GROUP("Group quota", 1);

   private final String comboBoxValue;
   private final int indexComboBox;

   QuotaIDTypeEnum(String comboBoxValue, int indexComboBox)
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

   static public String[] getValuesForComboBox()
   {
      QuotaIDTypeEnum[] types = QuotaIDTypeEnum.values();
      String[] retVal = new String[types.length];

      for (QuotaIDTypeEnum type : types)
      {
         retVal[type.getIndex()] = type.toString();
      }

      return retVal;
   }
}
