// ARAck mockups: shared sample data + render helpers
const SLOTS = [
  {n:'ARAck Utility',   t:'Built-in', on:true,  lat:0,     ara:false, k:'cyan'},
  {n:'Pitch Editor',    t:'VST3 · ARA2', on:true, lat:2.67, ara:true,  k:'or'},
  {n:'Linear Phase EQ', t:'VST3', on:true,  lat:42.67, ara:false, k:'blue'},
  {n:'De-esser',        t:'VST3', on:false, lat:1.33,  ara:false},
  {n:'Bus Compressor',  t:'VST3', on:true,  lat:0,     ara:false, k:'red'},
  {n:'Tape Saturation', t:'VST3', on:false, lat:0.67,  ara:false},
  {n:'Stereo Width',    t:'VST3', on:true,  lat:0,     ara:false},
  {n:'True Peak Limiter',t:'VST3',on:true,  lat:5.33,  ara:false},
  null,null,null,null
];
const POWER = '<svg viewBox="0 0 16 16"><path d="M8 1.5v6"/><path d="M4.3 4a5.5 5.5 0 1 0 7.4 0"/></svg>';
const LANES = [
  {p:'Pitch Editor · Formant',     v:'+1.5 st', k:'or',   seed:1},
  {p:'Linear Phase EQ · Band 3 Gain', v:'−2.4 dB', k:'blue', seed:2},
  {p:'Bus Compressor · Threshold', v:'−18.0 dB', k:'red', seed:3},
  {p:'ARAck Utility · Width',      v:'112 %',   k:'cyan', seed:4},
];
const KCOL = {or:'#ff5a1f',blue:'#3a86d4',red:'#e0322b',cyan:'#0f8a94'};
const fmtLat = l => l===0 ? '0.00' : l.toFixed(2);

function totalLatency(){ return SLOTS.filter(s=>s&&s.on).reduce((a,s)=>a+s.lat,0).toFixed(2); }

// deterministic automation curve: breakpoints, returns svg path (w×h)
function curve(seed,w,h,steps=14){
  let x=0, pts=[], r=seed*9301+49297;
  const rnd=()=>{r=(r*9301+49297)%233280;return r/233280;};
  let y=h*(0.3+rnd()*0.4);
  for(let i=0;i<=steps;i++){
    pts.push([x,y]);
    x+= w/steps;
    const jump = rnd();
    y = jump<0.25 ? y : Math.max(h*0.1,Math.min(h*0.9, y+(rnd()-0.5)*h*0.7));
  }
  return pts;
}
function laneSVG(lane,w,h,opts={}){
  const pts=curve(lane.seed,w,h);
  const col=KCOL[lane.k];
  const d=pts.map((p,i)=>(i?'L':'M')+p[0].toFixed(1)+' '+p[1].toFixed(1)).join(' ');
  const fill=d+` L ${w} ${h} L 0 ${h} Z`;
  let grid='';
  for(let i=1;i<16;i++){const gx=w*i/16;grid+=`<line x1="${gx}" y1="0" x2="${gx}" y2="${h}" stroke="var(--grid)" stroke-width="${i%4?1:2}"/>`;}
  const dots=pts.map(p=>`<rect x="${p[0]-3}" y="${p[1]-3}" width="6" height="6" fill="${opts.dark?'#0d0d0d':'#fff'}" stroke="${col}" stroke-width="1.5"/>`).join('');
  return `<svg width="100%" height="100%" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">${grid}<path d="${fill}" fill="${col}" opacity=".12"/><path d="${d}" fill="none" stroke="${col}" stroke-width="2"/>${opts.noDots?'':dots}</svg>`;
}
function ruler(el,bars=16,start=17){
  let h='';for(let i=0;i<bars;i+=1){h+=`<span style="left:${i/bars*100}%">${start+i}</span>`;}
  el.innerHTML=h+'<div class="playhead" style="left:38%"></div>';
}
// mock waveform for an ARA audio region
function waveSVG(w,h,col){
  let d='',r=7;const rnd=()=>{r=(r*9301+49297)%233280;return r/233280;};
  for(let x=0;x<w;x+=2){const env=0.25+0.75*Math.abs(Math.sin(x/90))*(0.6+rnd()*0.4);const a=env*h*0.45;d+=`M${x} ${h/2-a}V${h/2+a}`;}
  return `<svg width="100%" height="100%" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none"><path d="${d}" stroke="${col}" stroke-width="1.2"/></svg>`;
}
// mock pitch-editor plug-in view (note blobs over a waveform)
function pitchView(w,h,dark){
  let s=`<svg width="100%" height="100%" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">`;
  for(let i=0;i<12;i++){const y=h*i/12;s+=`<rect x="0" y="${y}" width="${w}" height="${h/12}" fill="${[1,3,6,8,10].includes(i)?(dark?'#121212':'#e3e1dc'):'transparent'}"/>`;}
  const notes=[[40,7,90],[150,6,60],[230,5,140],[400,7,70],[490,8,110],[620,6,80],[720,4,160],[900,5,70],[990,6,120]];
  notes.forEach(([x,n,len],i)=>{const sx=x*w/1100,sl=len*w/1100,y=h*n/12;
    s+=`<rect x="${sx}" y="${y+3}" width="${sl}" height="${h/12-6}" rx="5" fill="${i==4?'#ff5a1f':(dark?'#2a2a2a':'#c9c7c2')}" stroke="${i==4?'#ff5a1f':(dark?'#5c5b58':'#9d9b96')}"/>`;
    s+=`<path d="M${sx+4} ${y+h/24} q ${sl/4} -4 ${sl/2} 0 t ${sl/2-8} 0" stroke="${i==4?'#fff':(dark?'#8c8c88':'#5c5b58')}" fill="none" stroke-width="1.2"/>`;});
  return s+'</svg>';
}
// mock EQ view
function eqView(w,h,dark){
  let s=`<svg width="100%" height="100%" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">`;
  [0.2,0.4,0.6,0.8].forEach(f=>s+=`<line x1="0" y1="${h*f}" x2="${w}" y2="${h*f}" stroke="var(--grid)"/>`);
  [0.1,0.3,0.5,0.7,0.9].forEach(f=>s+=`<line x1="${w*f}" y1="0" x2="${w*f}" y2="${h}" stroke="var(--grid)"/>`);
  let d=`M0 ${h*0.5}`;for(let x=0;x<=w;x+=6){const y=h*0.5-60*Math.exp(-((x-w*0.18)**2)/(2*(w*0.05)**2))+50*Math.exp(-((x-w*0.55)**2)/(2*(w*0.07)**2))-35*Math.exp(-((x-w*0.82)**2)/(2*(w*0.1)**2));d+=` L${x} ${y}`;}
  s+=`<path d="${d} L${w} ${h} L0 ${h}Z" fill="#3a86d4" opacity=".12"/><path d="${d}" stroke="#3a86d4" stroke-width="2.5" fill="none"/>`;
  [[0.18,60,'1'],[0.55,-50,'3'],[0.82,35,'4']].forEach(([f,g,n],i)=>{s+=`<circle cx="${w*f}" cy="${h*0.5-g}" r="9" fill="${i==1?'#ff5a1f':(dark?'#0d0d0d':'#fff')}" stroke="${i==1?'#ff5a1f':'#3a86d4'}" stroke-width="2"/>`;});
  return s+'</svg>';
}
const LOGO = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="72 72 1188 288" role="img" aria-label="Miu" fill="currentColor"><polygon points="913.5 229.5 900 216 900 72 756.01 72 756.01 279 837 360 954 360 1066.5 247.5 1066.5 229.5 913.5 229.5"/><polygon points="1116 72 1116 216 1102.5 229.5 1095.75 236.25 990 342 990 360 1179 360 1260 279 1260 72 1116 72"/><polygon points="738.01 72 609.76 72 594.01 87.75 594.01 360 717.76 360 738.01 339.75 738.01 72"/><polygon points="576.01 180 576.01 72 432.01 72 324.01 180 216.01 72 72.01 72 72.01 180 252.01 360 396.01 360 576.01 180"/><polygon points="432.01 342 432.01 360 576.01 360 576.01 216 558.01 216 432.01 342"/><polygon points="90.01 216 72.01 216 72.01 360 216.01 360 216.01 342 90.01 216"/></svg>';
function header(el){el.querySelectorAll('.logo-svg').forEach(e=>e.innerHTML=LOGO);}
