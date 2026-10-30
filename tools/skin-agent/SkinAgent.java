// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.
package app.m2cl.skin;

import java.io.*;
import java.lang.instrument.Instrumentation;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.jar.JarFile;

/** A game-lifetime, loopback-only skin provider. No login or session endpoints. */
public final class SkinAgent {
    private final Properties settings;
    private final byte[] texture;
    private final String hash, prefix;
    private final KeyPair key;
    private final ServerSocket socket;
    private final ExecutorService workers;
    private final String root;

    private SkinAgent(Properties settings) throws Exception {
        this.settings=settings;
        texture=Files.readAllBytes(Paths.get(settings.getProperty("texture")));
        if(texture.length>1024*1024)throw new IOException("Skin exceeds size limit");
        hash=hex(MessageDigest.getInstance("SHA-256").digest(texture));
        if(!hash.equals(settings.getProperty("sha256")))throw new IOException("Skin checksum mismatch");
        KeyPairGenerator generator=KeyPairGenerator.getInstance("RSA");generator.initialize(2048);key=generator.generateKeyPair();
        prefix="/"+UUID.randomUUID().toString().replace("-","");
        socket=new ServerSocket();socket.bind(new InetSocketAddress(InetAddress.getByName("127.0.0.1"),0),16);
        root="http://127.0.0.1:"+socket.getLocalPort()+prefix;
        ThreadFactory daemon=r->{Thread t=new Thread(r,"M2CL local skin");t.setDaemon(true);return t;};
        workers=new ThreadPoolExecutor(0,4,30,TimeUnit.SECONDS,new SynchronousQueue<Runnable>(),daemon,new ThreadPoolExecutor.AbortPolicy());
        Thread accept=daemon.newThread(()->{
            while(!socket.isClosed())try{
                Socket client=socket.accept();client.setSoTimeout(5000);
                try{workers.execute(()->serve(client));}catch(RejectedExecutionException busy){client.close();}
            }catch(IOException ignored){}
        });accept.start();
    }
    public static void premain(String argument,Instrumentation instrumentation) throws Exception {
        Properties p=new Properties();try(Reader reader=Files.newBufferedReader(Paths.get(argument),StandardCharsets.UTF_8)){p.load(reader);}
        if(!p.getProperty("name","").matches("[A-Za-z0-9_]{1,16}")||!p.getProperty("uuid","").matches("[a-f0-9]{32}"))throw new IOException("Invalid local player");
        SkinAgent server=new SkinAgent(p);
        System.setProperty("m2cl.skin.api",server.root);
        System.setProperty("authlibinjector.side","client");
        System.setProperty("authlibinjector.noLogFile","true");
        System.setProperty("authlibinjector.noShowServerName","true");
        System.setProperty("authlibinjector.mojangNamespace","disabled");
        System.setProperty("authlibinjector.mojangAntiFeatures","enabled");
        System.setProperty("authlibinjector.usernameCheck","enabled");
        instrumentation.appendToSystemClassLoaderSearch(new JarFile(p.getProperty("injector")));
        Class.forName("moe.yushi.authlibinjector.Premain",true,ClassLoader.getSystemClassLoader())
            .getMethod("premain",String.class,Instrumentation.class).invoke(null,server.root,instrumentation);
        System.out.println("[M2CL] Local player skin ready ("+p.getProperty("model")+")");
    }
    private void serve(Socket client){
        try(Socket connection=client){
            InputStream input=connection.getInputStream();ByteArrayOutputStream head=new ByteArrayOutputStream();int b,state=0;
            while((b=input.read())!=-1){head.write(b);if(head.size()>16384)throw new IOException("Header too large");
                state=(state==0&&b=='\r')?1:(state==1&&b=='\n')?2:(state==2&&b=='\r')?3:(state==3&&b=='\n')?4:0;if(state==4)break;}
            String[] lines=new String(head.toByteArray(),StandardCharsets.US_ASCII).split("\r\n");
            String[] request=lines[0].split(" ");if(request.length!=3)return;
            String path=request[1].split("\\?",2)[0];
            if(!path.startsWith(prefix+"/")&&!path.equals(prefix)){respond(connection,404,"text/plain",new byte[0]);return;}
            path=path.substring(prefix.length());String method=request[0];
            if(method.equals("GET")&&(path.isEmpty()||path.equals("/"))){
                String pem="-----BEGIN PUBLIC KEY-----\n"+Base64.getEncoder().encodeToString(key.getPublic().getEncoded())+"\n-----END PUBLIC KEY-----";
                json(connection,"{\"meta\":{\"serverName\":\"M2CL Local Skin\",\"implementationName\":\"M2CL\",\"feature.no_mojang_namespace\":true},\"skinDomains\":[\"127.0.0.1\"],\"signaturePublickey\":"+quote(pem)+"}");
            }else if(method.equals("GET")&&path.equals("/sessionserver/session/minecraft/profile/"+settings.getProperty("uuid"))){
                json(connection,profile());
            }else if(method.equals("GET")&&path.equals("/textures/"+hash)){
                respond(connection,200,"image/png",texture);
            }else if(method.equals("POST")&&path.equals("/api/profiles/minecraft")){
                int length=0;for(String line:lines)if(line.toLowerCase(Locale.ROOT).startsWith("content-length:"))length=Integer.parseInt(line.substring(15).trim());
                if(length<0||length>4096)throw new IOException("Invalid body length");byte[] body=new byte[length];new DataInputStream(input).readFully(body);
                String names=new String(body,StandardCharsets.UTF_8);
                json(connection,names.contains(quote(settings.getProperty("name")))?"[{\"id\":"+quote(settings.getProperty("uuid"))+",\"name\":"+quote(settings.getProperty("name"))+"}]":"[]");
            }else if(path.contains("/join")||path.contains("/hasJoined")||path.startsWith("/authserver/")){
                respond(connection,403,"application/json","{\"error\":\"Skin provider only\"}".getBytes(StandardCharsets.UTF_8));
            }else if(method.equals("GET")&&path.startsWith("/sessionserver/session/minecraft/profile/"))respond(connection,204,"application/json",new byte[0]);
            else respond(connection,404,"application/json","{\"error\":\"NotFound\"}".getBytes(StandardCharsets.UTF_8));
        }catch(Exception ignored){}
    }
    private String profile()throws Exception{
        String name=quote(settings.getProperty("name")),id=quote(settings.getProperty("uuid"));
        String metadata="slim".equals(settings.getProperty("model"))?",\"metadata\":{\"model\":\"slim\"}":"";
        String payload="{\"timestamp\":"+System.currentTimeMillis()+",\"profileId\":"+id+",\"profileName\":"+name+",\"textures\":{\"SKIN\":{\"url\":"+quote(root+"/textures/"+hash)+metadata+"}}}";
        String value=Base64.getEncoder().encodeToString(payload.getBytes(StandardCharsets.UTF_8));
        Signature signature=Signature.getInstance("SHA1withRSA");signature.initSign(key.getPrivate());signature.update(value.getBytes(StandardCharsets.UTF_8));
        return "{\"id\":"+id+",\"name\":"+name+",\"properties\":[{\"name\":\"textures\",\"value\":"+quote(value)+",\"signature\":"+quote(Base64.getEncoder().encodeToString(signature.sign()))+"}]}";
    }
    private static String quote(String value){return "\""+value.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r")+"\"";}
    private static String hex(byte[] bytes){StringBuilder s=new StringBuilder();for(byte b:bytes)s.append(String.format("%02x",b&255));return s.toString();}
    private static void json(Socket client,String json)throws IOException{respond(client,200,"application/json; charset=utf-8",json.getBytes(StandardCharsets.UTF_8));}
    private static void respond(Socket client,int status,String type,byte[] body)throws IOException{
        OutputStream out=client.getOutputStream();out.write(("HTTP/1.1 "+status+" Response\r\nContent-Type: "+type+"\r\nContent-Length: "+body.length+"\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n").getBytes(StandardCharsets.US_ASCII));out.write(body);out.flush();
    }
}
