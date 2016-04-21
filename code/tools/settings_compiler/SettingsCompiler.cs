﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Reflection;
using System.CodeDom.Compiler;
using Microsoft.CSharp;

namespace SettingsCompiler
{
    class SettingsCompiler
    {
        static Assembly CompileSettings(string inputFilePath)
        {
            string fileName = Path.GetFileNameWithoutExtension(inputFilePath);

            string code = File.ReadAllText(inputFilePath);
            code = "using SettingsCompiler;\r\n\r\n" + "namespace " + fileName + "\r\n{\r\n" + code;
            code += "\r\n}";

            Dictionary<string, string> compilerOpts = new Dictionary<string, string> { { "CompilerVersion", "v4.0" } };
            CSharpCodeProvider compiler = new CSharpCodeProvider(compilerOpts);

            string[] sources = { code };
            CompilerParameters compilerParams = new CompilerParameters();
            compilerParams.GenerateInMemory = true;
            compilerParams.ReferencedAssemblies.Add("System.dll");
            compilerParams.ReferencedAssemblies.Add("SettingsCompilerAttributes.dll");
            CompilerResults results = compiler.CompileAssemblyFromSource(compilerParams, sources);
            if(results.Errors.HasErrors)
            {
                string errMsg = "Errors were returned from the C# compiler:\r\n\r\n";
                foreach(CompilerError compilerError in results.Errors)
                {
                    int lineNum = compilerError.Line - 4;
                    errMsg += inputFilePath + "(" + lineNum + "): " + compilerError.ErrorText + "\r\n";
                }
                throw new Exception(errMsg);
            }

            return results.CompiledAssembly;
        }

        static void ReflectType(Type settingsType, List<Setting> settings, List<Type> enumTypes,
                                string group)
        {
            object settingsInstance = Activator.CreateInstance(settingsType);

            BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
            FieldInfo[] fields = settingsType.GetFields(flags);
            foreach(FieldInfo field in fields)
            {
                foreach(Setting setting in settings)
                    if(setting.Name == field.Name)
                        throw new Exception(string.Format("Duplicate setting \"{0}\" detected", setting.Name));

                Type fieldType = field.FieldType;
                object fieldValue = field.GetValue(settingsInstance);
                if (fieldType == typeof(float))
                    settings.Add(new FloatSetting((float)fieldValue, field, group));
                else if (fieldType == typeof(int))
                    settings.Add(new IntSetting((int)fieldValue, field, group));
                else if (fieldType == typeof(bool))
                    settings.Add(new BoolSetting((bool)fieldValue, field, group));
                else if (fieldType.IsEnum)
                {
                    if (enumTypes.Contains(fieldType) == false)
                        enumTypes.Add(fieldType);
                    settings.Add(new EnumSetting(fieldValue, field, fieldType, group));
                }
                //else if(fieldType == typeof(Direction))
                //    settings.Add(new DirectionSetting((Direction)fieldValue, field, group));
                //else if(fieldType == typeof(Orientation))
                //    settings.Add(new OrientationSetting((Orientation)fieldValue, field, group));
                else if (fieldType == typeof(float3))
                    settings.Add(new Float3Setting((float3)fieldValue, field, group));
                else if (fieldType == typeof(float4))
                    settings.Add(new Float4Setting((float4)fieldValue, field, group));
                else if (fieldType == typeof(Color))
                    settings.Add(new ColorSetting((Color)fieldValue, field, group));
                else if (fieldType == typeof(string))
                    settings.Add(new StringSetting((string)fieldValue, field, group));
                else
                    throw new Exception("Invalid type for setting " + field.Name);
            }
        }

        static void ReflectSettings(Assembly assembly, Type baseType, List<Setting> settings, List<Type> enumTypes)
        {
            ReflectType(baseType, settings, enumTypes, "");

            Type[] nestedTypes = baseType.GetNestedTypes();
            foreach (Type nestedType in nestedTypes)
                ReflectType(nestedType, settings, enumTypes, nestedType.Name);
            
            //if(settingsType == null)
            //    throw new Exception("Settings file " + inputFilePath + " doesn't define a \"Settings\" class");

            //foreach (Type type in assembly.DefinedTypes)
            //{
            //    
            //}
        }

        static void WriteIfChanged(List<string> lines, string outputPath)
        {
            string outputText = "";
            foreach(string line in lines)
                outputText += line + "\r\n";

            string fileText = "";
            if(File.Exists(outputPath))
                fileText = File.ReadAllText(outputPath);

            int idx = fileText.IndexOf("// ================================================================================================");
            if(idx >= 0)
                outputText += "\r\n" + fileText.Substring(idx);

            if(fileText != outputText)
                File.WriteAllText(outputPath, outputText);
        }

        //public static void WriteEnumTypes(List<string> lines, List<Type> enumTypes)
        //{
        //    foreach(Type enumType in enumTypes)
        //    {
        //        if(enumType.GetEnumUnderlyingType() != typeof(int))
        //            throw new Exception("Invalid underlying type for enum " + enumType.Name + ", must be int");
        //        string[] enumNames = enumType.GetEnumNames();
        //        int numEnumValues = enumNames.Length;

        //        Array values = enumType.GetEnumValues();
        //        int[] enumValues = new int[numEnumValues];
        //        for(int i = 0; i < numEnumValues; ++i)
        //            enumValues[i] = (int)values.GetValue(i);

        //        lines.Add("enum class " + enumType.Name);
        //        lines.Add("{");
        //        for(int i = 0; i < values.Length; ++i)
        //            lines.Add("    " + enumNames[i] + " = " + enumValues[i] + ",");
        //        lines.Add("\r\n    NumValues");

        //        lines.Add("};\r\n");

        //        lines.Add("typedef EnumSettingT<" + enumType.Name + "> " + enumType.Name + "Setting;\r\n");
        //    }
        //}

        //public static void WriteEnumLabels(List<string> lines, List<Type> enumTypes)
        //{
        //    foreach(Type enumType in enumTypes)
        //    {
        //        string[] enumNames = enumType.GetEnumNames();
        //        int numEnumValues = enumNames.Length;
        //        string[] enumLabels = new string[numEnumValues];

        //        for(int i = 0; i < numEnumValues; ++i)
        //        {
        //            FieldInfo enumField = enumType.GetField(enumNames[i]);
        //            EnumLabelAttribute attr = enumField.GetCustomAttribute<EnumLabelAttribute>();
        //            enumLabels[i] = attr != null ? attr.Label : enumNames[i];
        //        }

        //        lines.Add("static const char* " + enumType.Name + "Labels[" + numEnumValues + "] =");
        //        lines.Add("{");
        //        foreach(string label in enumLabels)
        //            lines.Add("    \"" + label + "\",");

        //        lines.Add("};\r\n");
        //    }
        //}

        //static void GenerateHeader(List<Setting> settings, string outputName, string outputPath,
        //                           List<Type> enumTypes)
        //{
        //    List<string> lines = new List<string>();

        //    lines.Add("#pragma once");
        //    lines.Add("");
        //    lines.Add("#include \"SampleFramework11/PCH.h\"");
        //    lines.Add("#include \"SampleFramework11/Settings.h\"");
        //    lines.Add("#include \"SampleFramework11/GraphicsTypes.h\"");
        //    lines.Add("");
        //    lines.Add("using namespace SampleFramework11;");
        //    lines.Add("");

        //    WriteEnumTypes(lines, enumTypes);

        //    lines.Add("namespace " + outputName);
        //    lines.Add("{");

        //    uint numCBSettings = 0;
        //    foreach(Setting setting in settings)
        //    {
        //        setting.WriteDeclaration(lines);
        //        if(setting.UseAsShaderConstant)
        //            ++numCBSettings;
        //    }

        //    if(numCBSettings > 0)
        //    {
        //        lines.Add("");
        //        lines.Add(string.Format("    struct {0}CBuffer",  outputName));
        //        lines.Add("    {");

        //        uint cbSize = 0;
        //        foreach(Setting setting in settings)
        //            setting.WriteCBufferStruct(lines, ref cbSize);

        //        lines.Add("    };");
        //        lines.Add("");
        //        lines.Add(string.Format("    extern ConstantBuffer<{0}CBuffer> CBuffer;", outputName));
        //    }

        //    lines.Add("");
        //    lines.Add("    void Initialize(ID3D11Device* device);");
        //    lines.Add("    void UpdateCBuffer(ID3D11DeviceContext* context);");

        //    lines.Add("};");

        //    WriteIfChanged(lines, outputPath);
        //}

        //static void GenerateCPP(List<Setting> settings, string outputName, string outputPath,
        //                        List<Type> enumTypes)
        //{
        //    List<string> lines = new List<string>();

        //    lines.Add("#include \"PCH.h\"");
        //    lines.Add("#include \"" + outputName + ".h\"");
        //    lines.Add("");
        //    lines.Add("using namespace SampleFramework11;");
        //    lines.Add("");

        //    WriteEnumLabels(lines, enumTypes);

        //    lines.Add("namespace " + outputName);
        //    lines.Add("{");

        //    uint numCBSettings = 0;
        //    foreach(Setting setting in settings)
        //    {
        //        setting.WriteDefinition(lines);
        //        if(setting.UseAsShaderConstant)
        //            ++numCBSettings;
        //    }

        //    if(numCBSettings > 0)
        //    {
        //        lines.Add("");
        //        lines.Add(string.Format("    ConstantBuffer<{0}CBuffer> CBuffer;", outputName));
        //    }

        //    lines.Add("");
        //    lines.Add("    void Initialize(ID3D11Device* device)");
        //    lines.Add("    {");
        //    lines.Add("        TwBar* tweakBar = Settings.TweakBar();");
        //    lines.Add("");

        //    foreach(Setting setting in settings)
        //        setting.WriteInitialization(lines);

        //    if(numCBSettings > 0)
        //        lines.Add("        CBuffer.Initialize(device);");

        //    lines.Add("    }");

        //    lines.Add("");
        //    lines.Add("    void UpdateCBuffer(ID3D11DeviceContext* context)");
        //    lines.Add("    {");

        //    foreach(Setting setting in settings)
        //        setting.WriteCBufferUpdate(lines);

        //    if(numCBSettings > 0)
        //    {
        //        lines.Add("");
        //        lines.Add("        CBuffer.ApplyChanges(context);");
        //        lines.Add("        CBuffer.SetVS(context, 7);");
        //        lines.Add("        CBuffer.SetHS(context, 7);");
        //        lines.Add("        CBuffer.SetDS(context, 7);");
        //        lines.Add("        CBuffer.SetGS(context, 7);");
        //        lines.Add("        CBuffer.SetPS(context, 7);");
        //        lines.Add("        CBuffer.SetCS(context, 7);");
        //    }

        //    lines.Add("    }");

        //    lines.Add("}");

        //    WriteIfChanged(lines, outputPath);
        //}

        //static void GenerateHLSL(List<Setting> settings, string outputName, string outputPath,
        //                         List<Type> enumTypes)
        //{
        //    uint numCBSettings = 0;
        //    foreach(Setting setting in settings)
        //    {
        //        if(setting.UseAsShaderConstant)
        //            ++numCBSettings;
        //    }

        //    List<string> lines = new List<string>();

        //    if(numCBSettings == 0)
        //        WriteIfChanged(lines, outputPath);

        //    lines.Add(string.Format("cbuffer {0} : register(b7)", outputName));
        //    lines.Add("{");

        //    foreach(Setting setting in settings)
        //        setting.WriteHLSL(lines);

        //    lines.Add("}");
        //    lines.Add("");

        //    foreach(Type enumType in enumTypes)
        //    {
        //        string[] enumNames = enumType.GetEnumNames();
        //        Array enumValues = enumType.GetEnumValues();
        //        for(int i = 0; i < enumNames.Length; ++i)
        //        {
        //            string line = "static const int " + enumType.Name + "_";
        //            line += enumNames[i] + " = " + (int)enumValues.GetValue(i) + ";";
        //            lines.Add(line);
        //        }

        //        lines.Add("");
        //    }

        //    WriteIfChanged(lines, outputPath);
        //}

        static void GenerateAttribsCPP( List<Setting> attribs, string outputName, string outputPath )
        {
            List<string> lines = new List<string>();

            lines.Add( "#pragma once" );

            lines.Add( string.Format("#define BX_{0}_ATTRIBUTES_DECLARE \\", outputName.ToUpper() ) );
            foreach (Setting attr in attribs)
            {
                lines.Add(string.Format("static bx::AttributeIndex attr_{0}; \\", attr.Name ));
            }
            lines.Add("//");

            lines.Add(string.Format("#define BX_{0}_ATTRIBUTES_DEFINE \\", outputName.ToUpper()));
            foreach (Setting attr in attribs)
            {
                lines.Add(string.Format("bx::AttributeIndex {0}::attr_{1} = -1; \\", outputName, attr.Name));
            }
            lines.Add("//");


            List<string> tmpLines = new List<string>();
            lines.Add(string.Format("#define BX_{0}_ATTRIBUTES_CREATE \\", outputName.ToUpper() ));
            lines.Add( "{\\" );
            foreach (Setting attr in attribs)
            {
                tmpLines.Clear();
                attr.WriteGraphAttributeCreation(tmpLines);
                foreach (string line in tmpLines)
                {
                    lines.Add(string.Format( "attr_{0} = ", attr.Name ) + line + " \\");
                }

            }
            lines.Add("}");
            lines.Add("//");

            //lines.Add("{");
            //
            //foreach (Setting attr in attribs)
            //{
            //    attr.WriteGraphAttributeCreation(lines);
            //}
            //
            //lines.Add("}");

            WriteIfChanged(lines, outputPath);
        }

        static void Run(string[] args)
        {
            if(args.Length < 1)
                throw new Exception("Invalid command-line parameters");

            List<Setting> settings = new List<Setting>();
            List<Setting> attribs = new List<Setting>();
            List<Type> settingsEnumTypes = new List<Type>();
            List<Type> attribsEnumTypes = new List<Type>();

            string filePath = args[0];
            string fileName = Path.GetFileNameWithoutExtension(filePath);
            string outputDir = Path.GetDirectoryName(filePath);

            Assembly compiledAssembly = CompileSettings(filePath);




            {
                string filePathOnly = Path.GetFileNameWithoutExtension(filePath);
                Type settingsType = compiledAssembly.GetType(filePathOnly + ".Settings", false);
                Type attribsType = compiledAssembly.GetType(filePathOnly + ".Attributes", false);

                if (settingsType == null && attribsType == null)
                {
                    throw new Exception("Settings file " + filePath + " doesn't define a \"Settings\" or \"Attributes\" class");
                }

                if (settingsType != null)
                {
                    ReflectSettings(compiledAssembly, settingsType, settings, settingsEnumTypes);
                }
                if (attribsType != null)
                {
                    ReflectSettings(compiledAssembly, attribsType, attribs, attribsEnumTypes);

                    string attrOutputPath = Path.Combine(outputDir, fileName) + "_attributes.h";
                    GenerateAttribsCPP(attribs, fileName, attrOutputPath);

                    

                }

            }

            //ReflectSettings(compiledAssembly, filePath, settings, enumTypes);

            
            //string outputPath = Path.Combine(outputDir, fileName) + ".h";

            //GenerateHeader(settings, fileName, outputPath, enumTypes);

            //outputPath = Path.Combine(outputDir, fileName) + ".cpp";
            //GenerateCPP(settings, fileName, outputPath, enumTypes);

            //outputPath = Path.Combine(outputDir, fileName) + ".hlsl";
            //GenerateHLSL(settings, fileName, outputPath, enumTypes);

            // Generate a dummy file that MSBuild can use to track dependencies
            // Generate a dummy file that MSBuild can use to track dependencies
            string outputPath = Path.Combine(outputDir, fileName) + ".deps";
            File.WriteAllText(outputPath, "This file is output to allow MSBuild to track dependencies");
        }

        static int Main(string[] args)
        {

            if(Debugger.IsAttached)
            {
                Run(args);
            }
            else
            {
                try
                {
                    Run(args);
                }
                catch(Exception e)
                {
                    Console.WriteLine("An error ocurred during settings compilation:\n\n" + e.Message);
                    return 1;
                }
            }

            return 0;
        }
    }
}