using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Forms;

namespace anim_tool_gui
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        static String _cfgFilename = "anim_tool_settings.xml";
        XmlDocument _cfgDoc = null;



        private void _ReadConfig()
        {
            XmlDocument cfgXmlDoc = new XmlDocument();
            if (System.IO.File.Exists(_cfgFilename))
            {
                cfgXmlDoc.Load(_cfgFilename);
            }
            else
            {
                XmlNode rootNode = cfgXmlDoc.CreateElement("anim_tool");
                cfgXmlDoc.AppendChild(rootNode);
            }

            XmlNode srcDirNode = cfgXmlDoc.SelectSingleNode("//anim_tool/srcDir");
            if (srcDirNode != null)
            {
                srcDir.Text = srcDirNode.InnerText;
            }
            else
            {
                XmlNode node = cfgXmlDoc.CreateElement("srcDir");
                node.InnerText = "please select source directory";
                cfgXmlDoc.ChildNodes[0].AppendChild(node);
                srcDir.Text = node.InnerText;
            }

            XmlNode dstDirNode = cfgXmlDoc.SelectSingleNode("//anim_tool/dstDir");
            if (dstDirNode != null)
            {
                dstDir.Text = dstDirNode.InnerText;
            }
            else
            {
                XmlNode node = cfgXmlDoc.CreateElement("dstDir");
                node.InnerText = "please select destination directory";
                cfgXmlDoc.ChildNodes[0].AppendChild(node);
                dstDir.Text = node.InnerText;
            }


            XmlNode binDirNode = cfgXmlDoc.SelectSingleNode("//anim_tool/binDir");
            if( binDirNode != null )
            {
                binDir.Text = binDirNode.InnerText;
            }
            else
            {
                XmlNode node = cfgXmlDoc.CreateElement("binDir");
                node.InnerText = "please select destination directory";
                cfgXmlDoc.ChildNodes[0].AppendChild(node);
                binDir.Text = node.InnerText;
            }

            _cfgDoc = cfgXmlDoc;

        }

        private void _RefreshFileList()
        {
            string dir = srcDir.Text;
            if( System.IO.Directory.Exists( dir ))
            {
                string [] files = System.IO.Directory.GetFiles(dir);
                List<AnimSrcFile> animFiles = new List<AnimSrcFile>();

                foreach( string file in files )
                {
                    animFiles.Add(new AnimSrcFile() { Name = System.IO.Path.GetFileName(file) });
                }

                listView.ItemsSource = animFiles;
            }
            
        }

        public MainWindow()
        {
            InitializeComponent();
            _ReadConfig();
            _RefreshFileList();
        }
        
        private Boolean _SelectDirectory( ref System.String result )
        {
            
            FolderBrowserDialog folderDialog = new FolderBrowserDialog();
            folderDialog.SelectedPath = System.IO.Directory.GetCurrentDirectory();

            DialogResult dialogResult = folderDialog.ShowDialog();

            if (dialogResult.ToString() == "OK")
            {
                result = folderDialog.SelectedPath;
                if (!result.EndsWith("\\"))
                {
                    result += "\\";
                }
                return true;
            }

            return false;
        }

        private void selectSrcDir_Click(object sender, RoutedEventArgs e)
        {
            String txt = "";
            if( _SelectDirectory( ref txt ) )
            {
                srcDir.Text = txt;
                XmlNode node = _cfgDoc.SelectSingleNode("//anim_tool/srcDir");
                node.InnerText = txt;
                _cfgDoc.Save(_cfgFilename);
                _RefreshFileList();
            }
        }
        private void selectBinDir_Click(object sender, RoutedEventArgs e)
        {
            String txt = "";
            if (_SelectDirectory(ref txt))
            {
                binDir.Text = txt;
                XmlNode node = _cfgDoc.SelectSingleNode("//anim_tool/binDir");
                node.InnerText = txt;
                _cfgDoc.Save(_cfgFilename);
            }
        }
        private void selectDstDir_Click(object sender, RoutedEventArgs e)
        {
            String txt = "";
            if (_SelectDirectory(ref txt))
            {
                dstDir.Text = txt;
                XmlNode node = _cfgDoc.SelectSingleNode("//anim_tool/dstDir");
                node.InnerText = txt;
                _cfgDoc.Save(_cfgFilename);
            }
        }

        private void exportSkel_Click(object sender, RoutedEventArgs e)
        {
            if (listView.SelectedItem != null)
            {
                AnimSrcFile srcFile = (AnimSrcFile)listView.SelectedItem;
                System.Diagnostics.ProcessStartInfo startInfo = new System.Diagnostics.ProcessStartInfo();
                string inputFile = srcDir.Text + srcFile.Name;
                string outputFile = dstDir.Text + srcFile.Name.Replace(".bvh", ".skel");
                startInfo.Arguments = "skel " + inputFile + " " + outputFile;

                startInfo.FileName = binDir.Text + "anim_tool.exe";
                startInfo.UseShellExecute = true;
                startInfo.ErrorDialog = true;
                System.Diagnostics.Process process = new System.Diagnostics.Process();
                process.StartInfo = startInfo;

                try
                {
                    process.Start();
                }
                catch (System.Exception ex)
                {
                    System.Windows.MessageBox.Show(ex.Message);
                }
            }
        }

        private void exportAnim_Click(object sender, RoutedEventArgs e)
        {
            foreach (AnimSrcFile animFile in listView.SelectedItems)
            {
                System.Diagnostics.ProcessStartInfo startInfo = new System.Diagnostics.ProcessStartInfo();
                string inputFile = srcDir.Text + animFile.Name;
                string outputFile = dstDir.Text + animFile.Name.Replace(".bvh", ".anim");
                startInfo.Arguments = "anim " + inputFile + " " + outputFile;

                startInfo.FileName = binDir.Text + "anim_tool.exe";
                startInfo.UseShellExecute = true;
                startInfo.ErrorDialog = true;
                System.Diagnostics.Process process = new System.Diagnostics.Process();
                process.StartInfo = startInfo;

                try
                {
                    process.Start();
                }
                catch ( System.Exception ex )
                {
                    System.Windows.MessageBox.Show( ex.Message );
                }
            }
        }


        private class AnimSrcFile
        {
            public string Name { get; set; }
        };


    }
}
