grep -Pn 'Chrom(ium|e)\b (?!OS)' chrome/app/chromium_strings.grd chrome/app/settings_chromium_strings.grdp chrome/app/settings_strings.grdp|grep -v '<message' 
