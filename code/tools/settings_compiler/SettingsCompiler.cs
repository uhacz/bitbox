using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

using System.Reflection;
using System.CodeDom.Compiler;
using Microsoft.CSharp;

namespace SettingsCompiler
{
    class SettingsCompiler
    {
        static Assembly CompileSettings(string inputFilePath)
        {
            string appPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetName().CodeBase).Replace( "file:\\", "" );

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
            compilerParams.ReferencedAssemblies.Add( appPath +  "\\SettingsCompilerAttributes.dll");
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

        static void ReflectFields( List<Setting> settings, List<Type> enumTypes, string group, FieldInfo[] fields, object settingsInstance )
        {
            foreach (FieldInfo field in fields)
            {
                foreach (Setting setting in settings)
                    if (setting.Name == field.Name)
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

        static void ReflectType(Type settingsType, SettingsContainer settCnt, string group)
        {
            object settingsInstance = Activator.CreateInstance(settingsType);

            {
                BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
                FieldInfo[] fields = settingsType.GetFields(flags);
                ReflectFields(settCnt.Settings, settCnt.Enums, group, fields, settingsInstance);
            }

            {
                BindingFlags flags = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;
                FieldInfo[] fields = settingsType.GetFields(flags);
                ReflectFields(settCnt.DirectSettings, settCnt.DirectEnums, group, fields, settingsInstance);
            }

        }

        static void ReflectSettings( ref List<SettingsContainer> containers, Assembly assembly, Type baseType )
        {
            //ReflectType(baseType, settings, enumTypes, "");

            Type[] nestedTypes = baseType.GetNestedTypes();
            foreach (Type nestedType in nestedTypes)
            {
                SettingsContainer container = new SettingsContainer();
                container.Name = nestedType.Name;
                if( nestedType.BaseType.Name == "Object")
                    container.BaseName = "nodeType";
                else
                    container.BaseName = nestedType.BaseType.Name;
                ReflectType(nestedType, container, nestedType.Name);

                containers.Add(container);
            }
                
            
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

        static void GenerateAttribsCPP( List<string> lines, SettingsContainer settCnt )
        {
            lines.Add( string.Format("#define BX_{0}_ATTRIBUTES_DECLARE \\", settCnt.Name.ToUpper() ) );
            foreach (Setting attr in settCnt.Settings )
            {
                lines.Add(string.Format("static bx::AttributeIndex attr_{0}; \\", attr.Name ));
            }
            lines.Add("//");

            lines.Add(string.Format("#define BX_{0}_ATTRIBUTES_DEFINE \\", settCnt.Name.ToUpper()));
            foreach (Setting attr in settCnt.Settings )
            {
                lines.Add(string.Format("bx::AttributeIndex {0}::attr_{1} = -1; \\", settCnt.Name, attr.Name));
            }
            lines.Add("//");


            List<string> tmpLines = new List<string>();
            lines.Add(string.Format("#define BX_{0}_ATTRIBUTES_CREATE \\", settCnt.Name.ToUpper() ));
            lines.Add( "{\\" );
            foreach (Setting attr in settCnt.Settings )
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
            lines.Add("\n");

        }

        static void GenerateSchemaXMLHeader( List<string> lines, string nameSpace )
        {
            lines.Add("<?xml version=\"1.0\" encoding=\"utf-8\" ?>");
            lines.Add("<xs:schema");
            lines.Add("elementFormDefault=\"qualified\"");
            lines.Add(string.Format( "targetNamespace=\"{0}\"", nameSpace ));
            lines.Add(string.Format( "xmlns=\"{0}\"", nameSpace ));
            lines.Add("xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" >");
            lines.Add("\n");
        }
        static void GenerateSchemaXMLFooter(List<string> lines)
        {
            lines.Add("\n");
            lines.Add("</xs:schema>");
        }

        static void GenerateSchemaXML( List<string>lines, SettingsContainer settings )
        {
            //XmlSchemaSimpleType floatListType = new XmlSchemaSimpleType();
            //floatListType.Name = "floatListType";
            //XmlSchemaSimpleTypeList floatListTypeContent = new XmlSchemaSimpleTypeList();
            //floatListTypeContent.ItemTypeName = new XmlQualifiedName("float");
            //floatListType.Content = floatListTypeContent;

            //XmlSchemaSimpleType float3Type = new XmlSchemaSimpleType();
            //float3Type.Name = "float3_t";
            //XmlSchemaSimpleTypeRestriction float3TypeContent = new XmlSchemaSimpleTypeRestriction();
            //float3TypeContent.BaseType = floatListType;
            //XmlSchemaLengthFacet float3TypeLength = new XmlSchemaLengthFacet();
            //float3TypeLength.Value = "3";
            //float3TypeContent.Facets.Add(float3TypeLength);



            //XmlSchemaComplexType schemaType = new XmlSchemaComplexType();
            //schemaType.Name = settings.Name;
            //schemaType.Parent
            //XmlWriter xmlWriter = XmlWriter.Create(outputPath);
            //XmlSchemaAttribute

            //xmlWriter.WriteEndDocument();
            //xmlWriter.Close();

            //const string ns = "xs";

            //<xs:complexType name="LocatorNode">
            //    <xs:complexContent>
            //      <xs:extension base="nodeType">
            //        <xs:attribute name="pos" type="float3_t" default="0 0 0"/>
            //        <xs:attribute name="rot" type="float3_t" default="0 0 0"/>
            //        <xs:attribute name="scale" type="float3_t" default="1 1 1"/>
            //      </xs:extension>
            //    </xs:complexContent>
            //  </xs:complexType>

            lines.Add("<xs:complexType name=\"" + settings.Name + "\">" );
            lines.Add("\t<xs:complexContent>");
            lines.Add("\t\t<xs:extension base=\"" + settings.BaseName + "\">");

            foreach (Setting attr in settings.DirectSettings)
            {
                int before = lines.Count;
                attr.WriteSchemaAttribute(lines);

                for (int i = before; i < lines.Count; ++i)
                    lines[i] = "\t\t\t" + lines[i];
            }

            lines.Add("\t\t</xs:extension>");
            lines.Add("\t</xs:complexContent>");
            lines.Add("</xs:complexType>");
            lines.Add("\n");
        }

        static void Run(string[] args)
        {
            if(args.Length < 1)
                throw new Exception("Invalid command-line parameters");

            string filePath = args[0];
            string fileName = Path.GetFileNameWithoutExtension(filePath);
            string outputDir = Path.GetDirectoryName(filePath);

            Assembly compiledAssembly = CompileSettings(filePath);


            List<SettingsContainer> settingsContainers = new List<SettingsContainer>();
            List<SettingsContainer> attribsContainers = new List<SettingsContainer>();

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
                    ReflectSettings( ref settingsContainers, compiledAssembly, settingsType );
                }
                if (attribsType != null)
                {
                    ReflectSettings( ref attribsContainers, compiledAssembly, attribsType );

                    List<string> attrLines = new List<string>();
                    List<string> schemaLines = new List<string>();

                    attrLines.Add("#pragma once");
                    GenerateSchemaXMLHeader(schemaLines, "bitBox");

                    foreach ( SettingsContainer settCnt in attribsContainers )
                    {
                        GenerateAttribsCPP( attrLines, settCnt );
                        GenerateSchemaXML( schemaLines, settCnt );
                    }
                    GenerateSchemaXMLFooter(schemaLines);


                    string attrOutputPath = Path.Combine(outputDir, fileName ) + "_attributes.h";
                    WriteIfChanged(attrLines, attrOutputPath);

                    string outputSchemaDir = "";
                    try
                    {
                        string toolRoot = System.Environment.GetEnvironmentVariable("BX_TOOL_ROOT");
                        outputSchemaDir = Path.Combine(toolRoot, "SceneEditor/Schemas/");
                    }
                    catch
                    {
                        outputSchemaDir = outputDir;
                    }

                    string attrOutputPathSchema = Path.Combine(outputSchemaDir, fileName ) + "_schema.xsd";

                    WriteIfChanged(schemaLines, attrOutputPathSchema);
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