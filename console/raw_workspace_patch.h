#pragma once
// Replace only root metadata. Copy every original score/preset value verbatim: reparsing
// and reserializing a multi-MB project can round doubles and invalidate its fingerprint.
namespace RawWorkspace {
inline bool space(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
inline size_t skipSpace(const char *s,size_t n,size_t at){while(at<n&&space(s[at]))at++;return at;}
inline bool stringEnd(const char *s,size_t n,size_t &at){if(at>=n||s[at]!='"')return false;at++;while(at<n){char c=s[at++];if(c=='"')return true;if(c=='\\'){if(at>=n)return false;at++;}else if((unsigned char)c<32)return false;}return false;}
inline void append(String &out,const char *p,size_t n){
#ifdef THEATRE_HOST_TEST
 out.append(p,n);
#else
 out.concat(p,n);
#endif
}
inline String patch(const String &source,const char *const *keys,const String *values,size_t count){
 const char *s=source.c_str();size_t n=source.length(),at=skipSpace(s,n,0);if(at>=n||s[at++]!='{')return String();
 String out;size_t extra=0;for(size_t i=0;i<count;i++)extra+=strlen(keys[i])+values[i].length()+5;
#ifdef THEATRE_HOST_TEST
 out.reserve(n+extra+2);
#else
 if(!out.reserve(n+extra+2))return String();
#endif
 out+="{";bool comma=false,closed=false;
 for(;;){at=skipSpace(s,n,at);if(at>=n)return String();if(s[at]=='}'){at++;closed=true;break;}size_t start=at,keyStart=at+1;if(!stringEnd(s,n,at))return String();size_t keyLength=at-keyStart-1;
  bool replace=false;for(size_t i=0;i<count;i++)if(strlen(keys[i])==keyLength&&!memcmp(s+keyStart,keys[i],keyLength))replace=true;
  at=skipSpace(s,n,at);if(at>=n||s[at++]!=':')return String();at=skipSpace(s,n,at);size_t beginValue=at;int depth=0;
  while(at<n){char c=s[at];if(c=='"'){if(!stringEnd(s,n,at))return String();continue;}if(c=='{'||c=='['){depth++;at++;continue;}if(c=='}'||c==']'){if(depth==0)break;depth--;at++;continue;}if(c==','&&depth==0)break;at++;}
  if(at==beginValue||depth||at>=n)return String();size_t end=at;while(end>beginValue&&space(s[end-1]))end--;
  if(!replace){if(comma)out+=",";append(out,s+start,end-start);comma=true;}
  if(s[at]=='}'){at++;closed=true;break;}if(s[at++]!=',')return String();
 }
 if(!closed||skipSpace(s,n,at)!=n)return String();
 for(size_t i=0;i<count;i++){if(comma)out+=",";out+="\"";out+=keys[i];out+="\":";out+=values[i];comma=true;}out+="}";return out;
}
}
