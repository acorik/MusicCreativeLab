import json
from PIL import Image, ImageDraw, ImageFont
S='/tmp/mcl-shots/'; O='/tmp/claude-0/tut/img/'
R=json.load(open(S+'regions.json'))
F=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',30)
def box(r,pad=0): return [r[0]-pad,r[1]-pad,r[0]+r[2]+pad,r[1]+r[3]+pad]
spec={
 '01_tutoriales':('window',['tabs']),
 '02_pantalla_principal':('window',['browser','tabs','scenes','inspector','keyboard','play','energy','learn','audio']),
 '03_barra_superior':('top',['file','play','stop','rec','metro','tap','bpm','pos','energy','swing','riserbars','xfade','recset','panic','learn','audio','cpu']),
 '04_fila_pista':(('row',30),['rec','play','undo','x2','clear','mute','solo','mode','quant','bars']),
 '05_escenas':(('scenes',30),[]),
 '06_riser':('window',['scenes','energy']),
 '07_inspector_synth':(('inspector',10),['change','cutoff','morph','attack']),
 '08_inspector_drums':(('inspector',10),['punch']),
 '09_efectos':(('inspector',10),['addfx','depth']),
 '10_mixer':('window',['addfx']),
 '11_piano_roll':('window',['quant','dup']),
 '12_navegador':(('browser',10),['scan','free']),
 '13_menu_pista':('window',['addtrack']),
 '14_menu_learn':('window',['cutoff']),
 '15_learn_activo':('window',['learn','cutoff']),
 '16_controladores':('window',['learnmode']),
 '17_micro':('window',['row','input','monitor']),
 '18_proyecto_nuevo':('window',[]),
 '19_esperando_nota':(('row',30),['mode']),
 '20_grabando':('window',['row']),
 '21_loop_cerrado':(('row',30),[]),
 '22_piano_roll_drums':('window',[]),
 '23_rec_set':('top',['recset','cpu']),
}
for name,(crop,marks) in spec.items():
    reg=R[name]; win=reg['window']
    im=Image.open(S+name+'.png').convert('RGB')
    if crop=='window': c=box(win)
    elif crop=='top': c=[win[0],win[1],win[0]+win[2],win[1]+112]
    else: c=box(reg[crop[0]],crop[1])
    c=[max(c[0],win[0]),max(c[1],win[1]),min(c[2],win[0]+win[2]),min(c[3],win[1]+win[3])]
    d=ImageDraw.Draw(im)
    for i,m in enumerate(marks,1):
        if m not in reg: print('missing',name,m); continue
        b=box(reg[m],3); d.rounded_rectangle(b,radius=8,outline=(255,140,40),width=4)
        cx,cy=b[0]+4,b[1]+4
        if crop=='top': cx,cy=(b[0]+b[2])//2, b[3]-6
        r=20; cx=min(max(cx,c[0]+r),c[2]-r); cy=min(max(cy,c[1]+r),c[3]-r)
        d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=(255,140,40),outline=(20,20,20),width=3)
        d.text((cx,cy),str(i),font=F,fill=(15,15,15),anchor='mm')
    out=im.crop(c)
    out.save(O+name+'.png',optimize=True)
    print(name,out.size)
