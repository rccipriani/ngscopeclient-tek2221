using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Globalization;

public static class Bridge
{
    static string resource = "GPIB0::5::INSTR", replay;
    static string work = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Tek2221Ngscope", "latest");
    static Waveform last;
    static Waveform Capture()
    {
        if(replay != null) return Waveform.Load(replay);
        Console.WriteLine(DateTime.Now.ToString("o") + " Capture CH1 " + resource);
        Tek2221Capture.Run(work, true, resource, "CH1", "ACQ");
        string report = File.ReadAllText(Path.Combine(work,"capture-report.txt"));
        if(!report.Contains("PASS:") || report.Contains("restore failed"))
            throw new IOException("Native capture failed; see " + Path.Combine(work,"capture-report.txt"));
        var w = Waveform.Load(work);
        Console.WriteLine("Captured " + w.Samples.Length + " points, " + w.Bits + " bits; raw files in " + work);
        return w;
    }
    static string ReadLine(NetworkStream stream)
    {
        var line = new StringBuilder();
        for(int i=0; i<256; i++)
        {
            int c = stream.ReadByte();
            if(c < 0) return null;
            if(c == 10) return line.ToString().Trim();
            if(c < 32 && c != 13) throw new IOException("Invalid command");
            line.Append((char)c);
        }
        throw new IOException("Command too long");
    }
    static void Reply(NetworkStream stream, string text)
    {
        byte[] data = Encoding.ASCII.GetBytes(text + "\n");
        stream.Write(data,0,data.Length);
    }
    public static int Main(string[] args)
    {
        try
        {
            int port = 22210;
            for(int i=0; i<args.Length; i++)
            {
                if(i+1 >= args.Length) throw new ArgumentException("Options require values: --resource, --replay, --port, --work");
                string value = args[++i];
                switch(args[i-1])
                {
                    case "--resource": resource=value; break;
                    case "--replay": replay=Path.GetFullPath(value); break;
                    case "--port": port=int.Parse(value,CultureInfo.InvariantCulture); break;
                    case "--work": work=Path.GetFullPath(value); break;
                    default: throw new ArgumentException("Unknown option " + args[i-1]);
                }
            }
            if(replay != null) last=Waveform.Load(replay);
            if(replay == null && !resource.StartsWith("GPIB",StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException("Live prototype requires a GPIB VISA resource");
            // Single process owns the hardware and latest directory. No shared WaveStar state.
            using(var instance = new System.Threading.Mutex(false, "Tek2221NgscopeBridge"))
            {
                if(!instance.WaitOne(0)) throw new IOException("Another Tek2221 bridge is running");
                try
                {
                    var listener = new TcpListener(IPAddress.Loopback,port);
                    listener.Start();
                    Console.WriteLine("Tek2221 ngscope bridge v0.1 " + (replay != null ? "REPLAY (no hardware)" : "LIVE " + resource));
                    Console.WriteLine("Listening on 127.0.0.1:" + port + "; Ctrl+C to exit.");
                    while(true)
                    {
                        using(TcpClient client = listener.AcceptTcpClient())
                        {
                            client.NoDelay=true;
                            using(NetworkStream stream = client.GetStream())
                            {
                                stream.ReadTimeout=120000; stream.WriteTimeout=30000;
                                try
                                {
                                    string command;
                                    while((command=ReadLine(stream)) != null)
                                    {
                                        try
                                        {
                                            switch(command.ToUpperInvariant())
                                            {
                                                case "*IDN?": Reply(stream,"TEKTRONIX,2221-BRIDGE," + (replay != null ? "REPLAY" : "GPIB") + ",0.1"); break;
                                                case "META?": if(last==null) last=Capture(); Reply(stream,last.Header()); break;
                                                case "CAPTURE?":
                                                    last=Capture();
                                                    Reply(stream,last.Header());
                                                    // BinaryWriter emits IEEE754 float32 little endian; no trailing delimiter.
                                                    using(var writer=new BinaryWriter(stream,Encoding.ASCII,true))
                                                        foreach(float sample in last.Samples) writer.Write(sample);
                                                    break;
                                                default: Reply(stream,"ERR Unknown bridge command"); break;
                                            }
                                        }
                                        catch(Exception e)
                                        {
                                            Console.Error.WriteLine(e);
                                            Reply(stream,"ERR " + e.Message.Replace('\r',' ').Replace('\n',' '));
                                        }
                                    }
                                }
                                catch(IOException e) { Console.WriteLine("Client disconnected: " + e.Message); }
                                catch(SocketException e) { Console.WriteLine("Client disconnected: " + e.Message); }
                            }
                        }
                    }
                }
                finally { instance.ReleaseMutex(); }
            }
        }
        catch(Exception e) { Console.Error.WriteLine(e.Message); return 1; }
    }
}
