using System;
using System.IO;

public static class ParserTests
{
    static void Assert(bool value, string message) { if(!value) throw new Exception(message); }
    static void Reject(Action action) { try { action(); } catch(InvalidDataException) { return; } catch(NotSupportedException) { return; } throw new Exception("Expected rejection"); }
    public static void Main(string[] args)
    {
        string source=Path.GetFullPath(args[0]), temp=Path.Combine(Path.GetTempPath(),"tek2221-parser-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try
        {
            string preamble=File.ReadAllText(Path.Combine(source,"preamble.txt"));
            byte[] raw=File.ReadAllBytes(Path.Combine(source,"curve-response.bin"));
            File.WriteAllBytes(Path.Combine(temp,"curve-response.bin"),raw);
            File.WriteAllText(Path.Combine(temp,"preamble.txt"),preamble);
            var w=Waveform.Load(temp);
            Assert(w.Samples.Length==4096 && w.IntervalFs==5000000000L,"Y8 geometry");
            // Nonzero ground coordinates must affect voltage, not only display position.
            File.WriteAllText(Path.Combine(temp,"preamble.txt"),preamble.Replace("YOFF:0,","YOFF:10,"));
            var shifted=Waveform.Load(temp);
            Assert(Math.Abs((w.Samples[0]-shifted.Samples[0])-.008)<1e-7,"YOFF scaling");
            foreach(string change in new string[]{"PT.FMT:ENV","PT.FMT:XY","YUNIT:DIVS","PT.OFF:-10000","NR.PT:4000","XINCR:NaN"})
            {
                string[] fields=change.Split(':');
                string bad=System.Text.RegularExpressions.Regex.Replace(preamble,System.Text.RegularExpressions.Regex.Escape(fields[0])+@":[^,;]+",change);
                File.WriteAllText(Path.Combine(temp,"preamble.txt"),bad);
                Reject(delegate { Waveform.Load(temp); });
            }
            Console.WriteLine("PASS: nonzero YOFF and invalid metadata/unsupported formats");
        }
        finally { Directory.Delete(temp,true); }
    }
}
