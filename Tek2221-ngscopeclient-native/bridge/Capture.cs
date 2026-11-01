using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
public static class Tek2221Capture {
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall)] static extern int viOpenDefaultRM(out uint rm);
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall, CharSet=CharSet.Ansi)] static extern int viOpen(uint rm,string resource,uint access,uint timeout,out uint session);
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall)] static extern int viSetAttribute(uint session,uint attribute,uint value);
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall)] static extern int viWrite(uint session,byte[] bytes,uint count,out uint written);
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall)] static extern int viRead(uint session,[Out] byte[] bytes,uint count,out uint read);
 [DllImport("visa32.dll", CallingConvention=CallingConvention.StdCall)] static extern int viClose(uint session);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint GetModuleFileName(IntPtr module,StringBuilder path,int size);

 static void Check(int status,string operation){if(status<0)throw new IOException(operation+" failed: 0x"+unchecked((uint)status).ToString("X8"));}
 static byte[] Query(uint session,string command,Action<string> say,string output){
  say("TX "+command);byte[] bytes=Encoding.ASCII.GetBytes(command+"\n");uint count;
  Check(viWrite(session,bytes,(uint)bytes.Length,out count),"viWrite "+command);if(count!=bytes.Length)throw new IOException("Short write");
  using(var ms=new MemoryStream()){
   for(int i=0;i<16;i++){
    byte[] buffer=new byte[16384];int status=viRead(session,buffer,(uint)buffer.Length,out count);ms.Write(buffer,0,(int)count);
    if(output!=null)File.WriteAllBytes(output,ms.ToArray());say("RX bytes="+count+" status=0x"+unchecked((uint)status).ToString("X8"));Check(status,"viRead "+command);
    if(status!=0x3FFF0006)return ms.ToArray();
   }
   throw new IOException("Response exceeds capture limit");
  }
 }
 static void Write(uint session,string command,Action<string> say){say("TX "+command);byte[] bytes=Encoding.ASCII.GetBytes(command+"\n");uint n;Check(viWrite(session,bytes,(uint)bytes.Length,out n),"viWrite");if(n!=bytes.Length)throw new IOException("Short write");}
 public static byte[] ParseCurve(byte[] message){
  int marker=Array.IndexOf(message,(byte)'%');if(marker<0||marker>32||marker+3>message.Length)throw new InvalidDataException("No expected legacy CURVE % binary header; raw reply saved.");
  string prefix=Encoding.ASCII.GetString(message,0,marker).Trim();if(prefix!="CURVE"&&prefix!="CURV"&&prefix!="")throw new InvalidDataException("Unexpected curve prefix: "+prefix);
  int count=(message[marker+1]<<8)|message[marker+2];if(count<1||marker+3+count>message.Length)throw new InvalidDataException("Truncated binary curve block");
  int sum=0;for(int i=marker+1;i<marker+3+count;i++)sum+=message[i];if((sum&255)!=0)throw new InvalidDataException("Curve checksum mismatch");
  for(int i=marker+3+count;i<message.Length;i++)if(message[i]!=10&&message[i]!=13&&message[i]!=';'&&message[i]!=' ')throw new InvalidDataException("Unexpected bytes after curve block");
  byte[] data=new byte[count-1];Buffer.BlockCopy(message,marker+3,data,0,data.Length);return data;
 }
 public static string Exchange(string resource,string command,string query,bool identity,Action<string> log){uint rm=0,session=0;try{Check(viOpenDefaultRM(out rm),"viOpenDefaultRM");Check(viOpen(rm,resource,0,3000,out session),"viOpen");Check(viSetAttribute(session,0x3FFF001A,3000),"Set timeout");Check(viSetAttribute(session,0x3FFF0038,0),"Disable termination");if(command!=null){if(identity){string id=Encoding.ASCII.GetString(Query(session,"ID?",log,null));if(id.IndexOf("2221",StringComparison.OrdinalIgnoreCase)<0)throw new InvalidDataException("Expected Tek2221 before command");}Write(session,command,log);}if(query==null)return "Command sent";string response=Encoding.ASCII.GetString(Query(session,query,log,null)).Trim();log("REPLY "+response);return response;}finally{if(session!=0)viClose(session);if(rm!=0)viClose(rm);}}
 public static string ControlKey(int ident){switch(ident){case 110:return "SMO";case 111:return "VEC";case 112:return "WEI";case 113:return "TRIGC";case 114:return "HSR";case 115:return "LSR";case 116:return "REP";case 117:return "ROL";case 118:return "SCA";case 119:return "SAV";case 120:return "STOR";default:throw new ArgumentException("Unknown control");}}
 public static string[] ControlChoices(int ident){switch(ident){case 110:case 111:return new string[]{"ON","OFF"};case 112:return new string[]{"1","2","4","8","16","32","64","128","256"};case 114:case 116:return new string[]{"SAM","AVE","ACC"};case 115:case 118:return new string[]{"SAM","PEA","AVE","ACC"};case 117:return new string[]{"SAM","PEA"};default:return new string[0];}}
 public static string ValidateControl(int ident,string value){ControlKey(ident);if(ident>=119)throw new ArgumentException("Read-only scope status");value=(value??"").Trim().ToUpperInvariant();if(ident==113){int n;if(!Int32.TryParse(value,out n)||n<16||n>4080||n%4!=0)throw new ArgumentException("Trigger count must be 16..4080 in steps of 4");return n.ToString(System.Globalization.CultureInfo.InvariantCulture);}foreach(string choice in ControlChoices(ident))if(choice==value)return value;throw new ArgumentException("Invalid control value");}
 public static string ParseControl(int ident,string response){string key=ControlKey(ident);string pattern=ident==120?@"^STOR(?:E)?\s+(ON|OFF)\s*;?$":@"^ACQ(?:UISITION)?\s+"+key+@"[A-Z]*\s*:\s*([A-Z0-9]+)\s*;?$";var m=System.Text.RegularExpressions.Regex.Match(response.Trim(),pattern,System.Text.RegularExpressions.RegexOptions.IgnoreCase);if(!m.Success)throw new InvalidDataException("Unexpected control reply: "+response);string value=m.Groups[1].Value.ToUpperInvariant();if(ident>=119){if(value!="ON"&&value!="OFF")throw new InvalidDataException("Expected ON/OFF status");return value;}if(value.StartsWith("SAM"))value="SAM";else if(value.StartsWith("PEA"))value="PEA";else if(value.StartsWith("AVE"))value="AVE";else if(value.StartsWith("ACC"))value="ACC";return ValidateControl(ident,value);}
 public static string Control(string resource,int ident,string value,Action<string> logger=null){string key=ControlKey(ident);if(value!=null)value=ValidateControl(ident,value);uint rm=0,session=0;try{Check(viOpenDefaultRM(out rm),"viOpenDefaultRM");Check(viOpen(rm,resource,0,3000,out session),"viOpen");Check(viSetAttribute(session,0x3FFF001A,3000),"Set timeout");Check(viSetAttribute(session,0x3FFF0038,0),"Disable termination");Action<string> log=logger??delegate(string message){};if(value!=null){string id=Encoding.ASCII.GetString(Query(session,"ID?",log,null));if(id.IndexOf("2221",StringComparison.OrdinalIgnoreCase)<0)throw new InvalidDataException("Expected Tek 2221 before control write");Write(session,"ACQ "+key+":"+value,log);}string reply=Encoding.ASCII.GetString(Query(session,ident==120?"STOR?":"ACQ? "+key,log,null));string actual=ParseControl(ident,reply);if(value!=null&&actual!=value)throw new IOException("Scope did not accept "+key+":"+value+"; readback="+actual);return actual;}finally{if(session!=0)viClose(session);if(rm!=0)viClose(rm);}}
 public static bool ParseSavedState(string response){var m=System.Text.RegularExpressions.Regex.Match(response.Trim(),@"^ACQ(?:UISITION)?\s+SAV(?:E)?\s*:\s*(ON|OFF)\s*;?$",System.Text.RegularExpressions.RegexOptions.IgnoreCase);if(!m.Success)throw new InvalidDataException("Unexpected ACQ? SAVE response: "+response);return m.Groups[1].Value.ToUpperInvariant()=="ON";}
 public static bool SavedState(string resource){uint rm=0,session=0;try{Check(viOpenDefaultRM(out rm),"viOpenDefaultRM");Check(viOpen(rm,resource,0,3000,out session),"viOpen");Check(viSetAttribute(session,0x3FFF001A,3000),"Set timeout");Check(viSetAttribute(session,0x3FFF0038,0),"Disable termination");string response=Encoding.ASCII.GetString(Query(session,"ACQ? SAVE",delegate(string message){},null));return ParseSavedState(response);}finally{if(session!=0)viClose(session);if(rm!=0)viClose(rm);}}
 public static string Probe(string folder,string resource){Directory.CreateDirectory(folder);uint rm=0,session=0;try{Check(viOpenDefaultRM(out rm),"viOpenDefaultRM");Check(viOpen(rm,resource,0,3000,out session),"viOpen");Check(viSetAttribute(session,0x3FFF001A,3000),"Set timeout");Check(viSetAttribute(session,0x3FFF0038,0),"Disable termination");string id=Encoding.ASCII.GetString(Query(session,"ID?",delegate(string message){},Path.Combine(folder,"identity.txt"))).Trim();if(id.IndexOf("2221",StringComparison.OrdinalIgnoreCase)<0)throw new InvalidDataException("Expected Tek 2221, received: "+id);return id;}finally{if(session!=0)viClose(session);if(rm!=0)viClose(rm);}}
 public static void Run(string folder,bool quiet=false){Run(folder,quiet,"GPIB0::5::INSTR");}
 public static void Run(string folder,bool quiet,string resource){Run(folder,quiet,resource,"CH1");}
 public static void Run(string folder,bool quiet,string resource,string channel){Run(folder,quiet,resource,channel,"ACQ");}
 public static void Run(string folder,bool quiet,string resource,string channel,string source){
  if(source!="ACQ"&&source!="REF4")throw new ArgumentException("Expected ACQ or REF4");
  if(channel!="CH1"&&channel!="CH2")throw new ArgumentException("Expected CH1 or CH2");
  Directory.CreateDirectory(folder);using(var log=new StreamWriter(Path.Combine(folder,"capture-report.txt"),false)){
   log.AutoFlush=true;Action<string> say=s=>{if(!quiet)Console.WriteLine(s);log.WriteLine(s);};
   say("Tektronix 2221 native "+channel+" capture v2");say("Date: "+DateTime.Now.ToString("o"));say("Process bits: "+(IntPtr.Size*8));say("Resource: "+resource);
   uint rm=0,session=0;string restore=null;
   try{
    Check(viOpenDefaultRM(out rm),"viOpenDefaultRM");Check(viOpen(rm,resource,0,10000,out session),"viOpen");
    Check(viSetAttribute(session,0x3FFF001A,10000),"Set timeout");Check(viSetAttribute(session,0x3FFF0038,0),"Disable newline termination for binary reads");
    string id=Encoding.ASCII.GetString(Query(session,"ID?",say,Path.Combine(folder,"identity.txt"))).Trim();say("Identity: "+id);
    if(id.IndexOf("2221",StringComparison.OrdinalIgnoreCase)<0)throw new InvalidDataException("Identity did not contain 2221. Stopping before configuration changes.");
    string original=Encoding.ASCII.GetString(Query(session,"DATA?",say,Path.Combine(folder,"data-before.txt"))).Trim();say("Original data settings: "+original);
    if(original.StartsWith("DAT",StringComparison.OrdinalIgnoreCase)&&original.IndexOf('?')<0&&original.IndexOf('\n')<0&&original.IndexOf('\r')<0)restore=original;
    if(restore==null)throw new InvalidDataException("Could not retain a valid DATA response to restore. Stopping before configuration changes.");
    if(source=="REF4"){string state=Encoding.ASCII.GetString(Query(session,"REFD?",say,Path.Combine(folder,"reference-state.txt")));if(state.IndexOf("EMPTY",StringComparison.OrdinalIgnoreCase)>=0)throw new InvalidOperationException("REF4 is empty. Use the scope SAVE REF control before requesting this waveform; SAVE/CONT alone does not populate REF4.");}
    Write(session,"DATA SOURCE:"+source+",CHANNEL:"+channel+",ENCDG:BINARY",say);
    string preamble=Encoding.ASCII.GetString(Query(session,"WFMPRE?",say,Path.Combine(folder,"preamble.txt"))).Trim();say("Preamble: "+preamble);
    byte[] raw=Query(session,"CURVE?",say,Path.Combine(folder,"curve-response.bin"));byte[] samples=ParseCurve(raw);File.WriteAllBytes(Path.Combine(folder,"curve-data.bin"),samples);
    say("PASS: native binary block length and checksum verified. Sample payload bytes: "+samples.Length);
    say("First data bytes: "+BitConverter.ToString(samples,0,Math.Min(64,samples.Length)));
    say("Preamble and sample interpretation will be checked before building the translator.");
   }catch(Exception e){say(e.ToString());}
   finally{
    if(session!=0){if(restore!=null)try{Write(session,restore.TrimEnd(new char[]{';'}),say);say("Original DATA settings restore command sent.");}catch(Exception e){say("DATA settings restore failed: "+e.Message);}viClose(session);}
    if(rm!=0)viClose(rm);
   }
   say("Finished. Return the results folder as a ZIP.");
  }
 }
}
