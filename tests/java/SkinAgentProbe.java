// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.spec.X509EncodedKeySpec;
import java.util.*;
import java.util.regex.*;

/** Opt-in integration probe against the actual Mojang authlib on the classpath. */
public class SkinAgentProbe {
    static String field(String json,String name){Matcher m=Pattern.compile("\\\""+name+"\\\":\\\"(.*?)\\\"").matcher(json);if(!m.find())throw new AssertionError("Missing "+name);return m.group(1);}
    static byte[] fetch(String url)throws Exception{HttpURLConnection c=(HttpURLConnection)new URL(url).openConnection();c.setConnectTimeout(3000);c.setReadTimeout(3000);try(InputStream in=c.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] b=new byte[4096];int n;while((n=in.read(b))>0)out.write(b,0,n);return out.toByteArray();}finally{c.disconnect();}}
    public static void main(String[] args)throws Exception{
        String root=System.getProperty("m2cl.skin.api");if(root==null)throw new AssertionError("Agent did not start");
        String metadata=new String(fetch(root+"/"),StandardCharsets.UTF_8);
        String id=args[0],name=args[1],model=args[2];
        String profile=new String(fetch(root+"/sessionserver/session/minecraft/profile/"+id),StandardCharsets.UTF_8);
        String value=field(profile,"value"),signed=field(profile,"signature");String pem=field(metadata,"signaturePublickey").replace("\\n","").replace("-----BEGIN PUBLIC KEY-----","").replace("-----END PUBLIC KEY-----","");
        PublicKey key=KeyFactory.getInstance("RSA").generatePublic(new X509EncodedKeySpec(Base64.getDecoder().decode(pem)));
        Signature signature=Signature.getInstance("SHA1withRSA");signature.initVerify(key);signature.update(value.getBytes(StandardCharsets.UTF_8));if(!signature.verify(Base64.getDecoder().decode(signed)))throw new AssertionError("Invalid texture signature");
        String textures=new String(Base64.getDecoder().decode(value),StandardCharsets.UTF_8);if(!textures.contains(id)||!textures.contains(name))throw new AssertionError("Wrong player identity");
        if(textures.contains("\"model\":\"slim\"")!=model.equals("slim"))throw new AssertionError("Wrong model");
        byte[] png=fetch(field(textures,"url"));if(png.length<8||png[0]!=(byte)137||png[1]!=80)throw new AssertionError("Missing PNG");
        HttpURLConnection join=(HttpURLConnection)new URL(root+"/sessionserver/session/minecraft/join").openConnection();join.setRequestMethod("POST");if(join.getResponseCode()!=403)throw new AssertionError("Skin server must not authenticate multiplayer sessions");join.disconnect();
        Class<?> auth=Class.forName("com.mojang.authlib.yggdrasil.YggdrasilAuthenticationService");Object service=auth.getConstructor(Proxy.class).newInstance(Proxy.NO_PROXY);Object session=auth.getMethod("createMinecraftSessionService").invoke(service);Class<?> api=Class.forName("com.mojang.authlib.minecraft.MinecraftSessionService");Class<?> gameProfile=Class.forName("com.mojang.authlib.GameProfile");
        try{auth.getMethod("createUserApiService",String.class).invoke(service,"0");}
        catch(java.lang.reflect.InvocationTargetException offline){if(!(offline.getCause() instanceof Exception)||!offline.getCause().getClass().getName().startsWith("com.mojang.authlib.exceptions."))throw offline;}
        UUID uuid=UUID.fromString(id.replaceFirst("(........)(....)(....)(....)(............)","$1-$2-$3-$4-$5"));
        try{
            Object input=gameProfile.getConstructor(UUID.class,String.class).newInstance(uuid,name);Object filled=api.getMethod("fillProfileProperties",gameProfile,boolean.class).invoke(session,input,true);
            Map<?,?> result=(Map<?,?>)api.getMethod("getTextures",gameProfile,boolean.class).invoke(session,filled,true);if(result.isEmpty())throw new AssertionError("Mojang authlib returned no skin");
        }catch(NoSuchMethodException modern){
            Object result=api.getMethod("fetchProfile",UUID.class,boolean.class).invoke(session,uuid,true);if(result==null)throw new AssertionError("Mojang authlib returned no profile");Object profileObject=result.getClass().getMethod("profile").invoke(result);Object resultTextures=api.getMethod("getTextures",gameProfile).invoke(session,profileObject);Object skin=resultTextures.getClass().getMethod("skin").invoke(resultTextures);if(skin==null)throw new AssertionError("Mojang authlib returned no skin");
        }
        System.out.println("PASS: signed "+model+" skin, PNG, stable UUID, rejected join, real Mojang authlib");
    }
}
