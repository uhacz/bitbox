﻿using System;
using System.Reflection;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;

namespace SettingsCompiler
{
    public enum SettingType
    {
        Float,
        Int,
        Float3,
        Float4,
        Bool,
        Enum,

        //Direction,
        //Orientation,
        Color,
        String,
    }

    public struct EnumValue
    {
        public string Name;
        public string Label;

        public EnumValue(string name, string label)
        {
            this.Name = name;
            this.Label = label;
        }
    }

    public abstract class Setting
    {
        public string Name;
        public string DisplayName = "";
        public SettingType Type;
        public string Group = "";
        public String HelpText = "";
        public bool UseAsShaderConstant = true;

        public Setting(FieldInfo field, SettingType type, string group)
        {
            Type = type;
            Name = field.Name;
            Group = group;
            DisplayNameAttribute.GetDisplayName(field, ref DisplayName);
            HelpTextAttribute.GetHelpText(field, ref HelpText);
            GroupAttribute.GetGroup(field, ref Group);
            UseAsShaderConstant = UseAsShaderConstantAttribute.UseFieldAsShaderConstant(field);
        }

        public abstract void WriteGraphAttributeCreation(List<string> lines);
        public abstract void WriteSchemaAttribute( List<string> lines );

        public static string schemaAttributeFormatString1 = "<xs:attribute name=\"{0}\" type=\"{1}\" default=\"{2}\"/>";
        public static string schemaAttributeFormatString2 = "<xs:attribute name=\"{0}\" type=\"{1}\" default=\"{2} {3}\"/>";
        public static string schemaAttributeFormatString3 = "<xs:attribute name=\"{0}\" type=\"{1}\" default=\"{2} {3} {4}\"/>";
        public static string schemaAttributeFormatString4 = "<xs:attribute name=\"{0}\" type=\"{1}\" default=\"{2} {3} {4} {5}\"/>";

        public static string FloatString(float num )
        {
            return num.ToString( "F4", CultureInfo.InvariantCulture) + "f";
        }

        public static string MakeParameter(float parameter)
        {
            return ", " + FloatString(parameter);
        }

        public static string MakeParameter(string parameter)
        {
            return ", \"" + parameter + "\"";
        }

        public static string MakeParameter(int parameter)
        {
            return ", " + parameter;
        }

        public static string MakeParameter(bool parameter)
        {
            return ", " + parameter.ToString().ToLower();
        }

        public static string MakeParameter(float3 parameter )
        {
            return ", float3_t(" + FloatString(parameter.x) + ", " +
                                   FloatString(parameter.y) + ", " +
                                   FloatString(parameter.z) + ")";
        }

        public static string MakeParameter(float4 parameter)
        {
            return ", float4_t(" + FloatString(parameter.x) + ", " +
                                   FloatString(parameter.y) + ", " +
                                   FloatString(parameter.z) + ", " +
                                   FloatString(parameter.w) + ")";
        }

        //public static string MakeParameter(Direction parameter)
        //{
        //    return ", Float3(" + FloatString(parameter.X) + ", " +
        //                         FloatString(parameter.Y) + ", " +
        //                         FloatString(parameter.Z) + ")";
        //}

        //public static string MakeParameter(Orientation parameter)
        //{
        //    return ", Quaternion(" + FloatString(parameter.X) + ", " +
        //                             FloatString(parameter.Y) + ", " +
        //                             FloatString(parameter.Z) + ", " +
        //                             FloatString(parameter.W) + ")";
        //}

        public static string MakeParameter(Color parameter)
        {
            return ", Float3(" + FloatString(parameter.R) + ", " +
                                 FloatString(parameter.G) + ", " +
                                 FloatString(parameter.B) + ")";
        }

        public static string SettingType_SchemaName(SettingType type)
        {
            switch (type)
            {
                case SettingType.Float: return "xs:float";
                case SettingType.Int: return "xs:int";
                case SettingType.Float3: return "float3_t";
                case SettingType.Float4: return "float4_t";
                case SettingType.Bool: return "xs:boolean";
                case SettingType.Enum: return "xs:int";
                case SettingType.Color: return "color_t";
                case SettingType.String: return "xs:string";
                default:
                    throw new Exception("Unknown Settings type" );
            }
        }

    }

    public class StringSetting : Setting
    {
        public string Value = "";

        public StringSetting( string value, FieldInfo field, string group )
            : base( field, SettingType.String, group )
        {
            Value = value;
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddString( typeIndex" + MakeParameter( Name ) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add( string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString1, Name, Setting.SettingType_SchemaName(Type), Value.ToString() ) );
        }
    }

    public class FloatSetting : Setting
    {
        public float Value = 0.0f;
        public float MinValue = float.MinValue;
        public float MaxValue = float.MaxValue;
        public float StepSize = 0.01f;

        public FloatSetting(float value, FieldInfo field, string group)
            : base(field, SettingType.Float, group)
        {
            Value = value;
            MinValueAttribute.GetMinValue(field, ref MinValue);
            MaxValueAttribute.GetMaxValue(field, ref MaxValue);
            StepSizeAttribute.GetStepSize(field, ref StepSize);
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddFloat( typeIndex" + MakeParameter(Name) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString1, Name, Setting.SettingType_SchemaName(Type), Value ));
        }
    }

    public class IntSetting : Setting
    {
        public int Value = 0;
        public int MinValue = int.MinValue;
        public int MaxValue = int.MaxValue;

        public IntSetting(int value, FieldInfo field, string group)
            : base(field, SettingType.Int, group)
        {
            Value = value;
            MinValueAttribute.GetMinValue(field, ref MinValue);
            MaxValueAttribute.GetMaxValue(field, ref MaxValue);
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddInt( typeIndex" + MakeParameter(Name) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString1, Name, Setting.SettingType_SchemaName(Type), Value ) );
        }
    }

    public class BoolSetting : Setting
    {
        public bool Value = false;

        public BoolSetting(bool value, FieldInfo field, string group)
            : base(field, SettingType.Bool, group)
        {
            Value = value;
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddInt( typeIndex" + MakeParameter(Name) + MakeParameter(Value ? 1 : 0) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString1, Name, Setting.SettingType_SchemaName(Type), Value.ToString().ToLower() ));
        }
    }

    public class EnumSetting : Setting
    {
        public object Value;
        public Type EnumType;
        public string EnumTypeName;
        public int NumEnumValues = 0;

        public EnumSetting(object value, FieldInfo field, Type enumType, string group)
            : base(field, SettingType.Enum, group)
        {
            Value = value;
            EnumType = enumType;
            NumEnumValues = EnumType.GetEnumValues().Length;
            EnumTypeName = EnumType.Name;
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddInt( typeIndex" + MakeParameter(Name) + ", 0 );");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString1, Name, Setting.SettingType_SchemaName(Type), Value.ToString() ));
        }
        //public override void WriteDeclaration(List<string> lines)
        //{
        //    lines.Add("    extern " + EnumTypeName + "Setting " + Name + ";");
        //}

        //public override void WriteDefinition(List<string> lines)
        //{
        //    lines.Add("    " + EnumTypeName + "Setting " + Name + ";");
        //}

        //private string MakeEnumParameter(object value)
        //{
        //    string parameter = EnumTypeName + "::" + EnumType.GetEnumName(value);
        //    return ", " + parameter;
        //}

        //public override void WriteInitialization(List<string> lines)
        //{
        //    string paramString = "tweakBar";
        //    paramString += MakeParameter(Name);
        //    paramString += MakeParameter(Group);
        //    paramString += MakeParameter(DisplayName);
        //    paramString += MakeParameter(HelpText);
        //    paramString += MakeEnumParameter(Value);
        //    paramString += MakeParameter(NumEnumValues);
        //    paramString += ", " + EnumTypeName + "Labels";
        //    lines.Add("        " + Name + ".Initialize(" + paramString + ");");
        //    lines.Add("        Settings.AddSetting(&" + Name + ");");
        //    lines.Add("");
        //}
    }

    //public class DirectionSetting : Setting
    //{
    //    public Direction Value = new Direction(0.0f, 0.0f, 1.0f);

    //    public DirectionSetting(Direction value, FieldInfo field, string group)
    //        : base(field, SettingType.Direction, group)
    //    {
    //        Value = value;
    //    }

    //    public override void WriteDeclaration(List<string> lines)
    //    {
    //        lines.Add("    extern DirectionSetting " + Name + ";");
    //    }

    //    public override void WriteDefinition(List<string> lines)
    //    {
    //        lines.Add("    DirectionSetting " + Name + ";");
    //    }

    //    public override void WriteInitialization(List<string> lines)
    //    {
    //        string paramString = "tweakBar";
    //        paramString += MakeParameter(Name);
    //        paramString += MakeParameter(Group);
    //        paramString += MakeParameter(DisplayName);
    //        paramString += MakeParameter(HelpText);
    //        paramString += MakeParameter(Value);
    //        lines.Add("        " + Name + ".Initialize(" + paramString + ");");
    //        lines.Add("        Settings.AddSetting(&" + Name + ");");
    //        lines.Add("");
    //    }
    //}

    //public class OrientationSetting : Setting
    //{
    //    public Orientation Value = Orientation.Identity;

    //    public OrientationSetting(Orientation value, FieldInfo field, string group)
    //        : base(field, SettingType.Orientation, group)
    //    {
    //        Value = value;
    //    }

    //    public override void WriteDeclaration(List<string> lines)
    //    {
    //        lines.Add("    extern OrientationSetting " + Name + ";");
    //    }

    //    public override void WriteDefinition(List<string> lines)
    //    {
    //        lines.Add("    OrientationSetting " + Name + ";");
    //    }

    //    public override void WriteInitialization(List<string> lines)
    //    {
    //        string paramString = "tweakBar";
    //        paramString += MakeParameter(Name);
    //        paramString += MakeParameter(Group);
    //        paramString += MakeParameter(DisplayName);
    //        paramString += MakeParameter(HelpText);
    //        paramString += MakeParameter(Value);
    //        lines.Add("        " + Name + ".Initialize(" + paramString + ");");
    //        lines.Add("        Settings.AddSetting(&" + Name + ");");
    //        lines.Add("");
    //    }
    //}

    public class ColorSetting : Setting
    {
        public Color Value = new Color(1.0f, 1.0f, 1.0f);
        //public bool HDR = false;
        public float MinValue = float.MinValue;
        public float MaxValue = float.MaxValue;
        public float StepSize = 0.01f;

        public ColorSetting(Color value, FieldInfo field, string group)
            : base(field, SettingType.Color, group)
        {
            Value = value;
            //HDR = HDRAttribute.HDRColor(field);
            MinValueAttribute.GetMinValue(field, ref MinValue);
            MaxValueAttribute.GetMaxValue(field, ref MaxValue);
            StepSizeAttribute.GetStepSize(field, ref StepSize);
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddFloat3( typeIndex" + MakeParameter(Name) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString3, Name, Setting.SettingType_SchemaName(Type), Value.R, Value.G, Value.B));
        }
        //public override void WriteDeclaration(List<string> lines)
        //{
        //    lines.Add("    extern ColorSetting " + Name + ";");
        //}

        //public override void WriteDefinition(List<string> lines)
        //{
        //    lines.Add("    ColorSetting " + Name + ";");
        //}

        //public override void WriteInitialization(List<string> lines)
        //{
        //    string paramString = "tweakBar";
        //    paramString += MakeParameter(Name);
        //    paramString += MakeParameter(Group);
        //    paramString += MakeParameter(DisplayName);
        //    paramString += MakeParameter(HelpText);
        //    paramString += MakeParameter(Value);
        //    paramString += MakeParameter(HDR);
        //    paramString += MakeParameter(MinValue);
        //    paramString += MakeParameter(MaxValue);
        //    paramString += MakeParameter(StepSize);
        //    lines.Add("        " + Name + ".Initialize(" + paramString + ");");
        //    lines.Add("        Settings.AddSetting(&" + Name + ");");
        //    lines.Add("");
        //}
    }

    public class Float3Setting : Setting
    {
        public float3 Value = new float3(0.0f);
        public float MinValue = float.MinValue;
        public float MaxValue = float.MaxValue;
        public float StepSize = 0.01f;

        public Float3Setting(float3 value, FieldInfo field, string group)
            : base(field, SettingType.Float3, group)
        {
            Value = value;
            MinValueAttribute.GetMinValue(field, ref MinValue);
            MaxValueAttribute.GetMaxValue(field, ref MaxValue);
            StepSizeAttribute.GetStepSize(field, ref StepSize);
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddFloat3( typeIndex" + MakeParameter(Name) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString3, Name, Setting.SettingType_SchemaName(Type), Value.x, Value.y, Value.z ));
        }
    }

    public class Float4Setting : Setting
    {
        public float4 Value = new float4(0.0f);
        public float MinValue = float.MinValue;
        public float MaxValue = float.MaxValue;
        public float StepSize = 0.01f;

        public Float4Setting(float4 value, FieldInfo field, string group)
            : base(field, SettingType.Float3, group)
        {
            Value = value;
            MinValueAttribute.GetMinValue(field, ref MinValue);
            MaxValueAttribute.GetMaxValue(field, ref MaxValue);
            StepSizeAttribute.GetStepSize(field, ref StepSize);
        }

        public override void WriteGraphAttributeCreation(List<string> lines)
        {
            lines.Add("bx::nodeAttributeAddFloat4( typeIndex" + MakeParameter(Name) + MakeParameter(Value) + ");");
        }
        public override void WriteSchemaAttribute(List<string> lines)
        {
            lines.Add(string.Format(CultureInfo.InvariantCulture, Setting.schemaAttributeFormatString4, Name, Setting.SettingType_SchemaName(Type), Value.x, Value.y, Value.z, Value.w ));
        }
    }


    public class SettingsContainer
    {
        public string Name { set; get; }
        public string BaseName { set; get; }

        public List<Setting> Settings
        {
            get { return m_settings; }
        }
        public List<Type> Enums
        {
            get { return m_enums; }
        }

        public List<Setting> DirectSettings
        {
            get { return m_directSettings; }
        }
        public List<Type> DirectEnums
        {
            get { return m_directEnums; }
        }

        /// <summary>
        /// settings from entire hierarchy (flattened)
        /// </summary>
        private List<Setting> m_settings = new List<Setting>();
        private List<Type> m_enums = new List<Type>();

        /// <summary>
        /// settings only from this class
        /// </summary>
        private List<Setting> m_directSettings = new List<Setting>();
        private List<Type> m_directEnums = new List<Type>();

    }
}