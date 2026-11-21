const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage();
await p.goto('file:///tmp/claude-0/tut/guia.html',{waitUntil:'networkidle'});
await p.pdf({path:'/tmp/claude-0/tut/guia.pdf',format:'A4',printBackground:true,preferCSSPageSize:true,displayHeaderFooter:true,headerTemplate:'<span></span>',footerTemplate:'<div style="font-size:8px;width:100%;text-align:center;color:#888">Music Creative Lab - Guia ilustrada &middot; <span class="pageNumber"></span>/<span class="totalPages"></span></div>'});
await b.close();})();
