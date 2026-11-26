#!/usr/bin/env python3
"""Read-only bounded ZIP and PE inventory. No executable is launched."""
import hashlib, json, pathlib, stat, struct, sys, zipfile

def digest(b): return hashlib.sha256(b).hexdigest()
def inventory(p):
    with zipfile.ZipFile(p) as z:
        entries=z.infolist(); seen=set()
        if len(entries)>50000 or sum(i.file_size for i in entries)>4*1024**3: raise ValueError('archive exceeds inspection bounds')
        for i in entries:
            name=i.filename; parts=pathlib.PurePosixPath(name).parts
            if not parts or name.startswith('/') or '..' in parts or '\\' in name or ':' in name or stat.S_ISLNK(i.external_attr>>16) or name.rstrip('/').casefold() in seen: raise ValueError(f'unsafe entry: {name}')
            seen.add(name.rstrip('/').casefold())
        if z.testzip() is not None: raise ValueError('bad CRC')
        return {'name':p.name,'size':p.stat().st_size,'sha256':digest(p.read_bytes()),'entries':[{'name':i.filename,'size':i.file_size,'sha256':digest(z.read(i))} for i in entries if not i.is_dir()]}

def pe(b):
    def u(fmt,o): return struct.unpack_from('<'+fmt,b,o)
    if b[:2]!=b'MZ': raise ValueError('not MZ')
    p=u('I',60)[0]; o=p+24
    if b[p:p+4]!=b'PE\0\0' or u('H',o)[0]!=0x10b: raise ValueError('not PE32')
    machine,n=u('HH',p+4); s=o+u('H',p+20)[0]
    sections=[u('8sIIIIIIHHI',s+40*i) for i in range(n)]
    def off(rva):
        for x in sections:
            if x[2]<=rva<x[2]+x[3]:
                k=x[4]+rva-x[2]
                if k<len(b): return k
        raise ValueError(f'unmapped RVA {rva:x}')
    def string(rva):
        k=off(rva); e=b.index(0,k,min(len(b),k+4096)); return b[k:e].decode('ascii','replace')
    imports=[]; ir,sz=u('II',o+104)
    for i in range(sz//20):
        lut,_,_,name,iat=u('5I',off(ir)+20*i)
        if not name: break
        names=[]
        for j in range(10000):
            v=u('I',off((lut or iat)+4*j))[0]
            if not v: break
            names.append(f'#{v&65535}' if v&0x80000000 else string(v+2))
        else: raise ValueError('unterminated imports')
        imports.append({'dll':string(name),'iat_rva':hex(iat),'symbols':names})
    else:
        if ir: raise ValueError('unterminated import table')
    exports=[]; er,_=u('II',o+96)
    if er:
        _,_,_,_,_,base,nf,nn,ft,nt,ot=u('IIHHIIIIIII',off(er))
        for i in range(nn):
            idx=u('H',off(ot)+2*i)[0]
            if idx>=nf: raise ValueError('bad export ordinal')
            exports.append({'name':string(u('I',off(nt)+4*i)[0]),'ordinal':base+idx,'rva':hex(u('I',off(ft)+4*idx)[0])})
    return {'sha256':digest(b),'machine':hex(machine),'image_base':hex(u('I',o+28)[0]),'entry_rva':hex(u('I',o+16)[0]),'image_size':hex(u('I',o+56)[0]),'sections':[{'name':x[0].rstrip(b'\0').decode(),'rva':hex(x[2]),'raw_offset':hex(x[4]),'flags':hex(x[-1])} for x in sections],'imports':imports,'exports':exports}
if __name__=='__main__':
    p=pathlib.Path(sys.argv[1]); print(json.dumps(inventory(p) if p.suffix.lower()=='.zip' else pe(p.read_bytes()),indent=2))
