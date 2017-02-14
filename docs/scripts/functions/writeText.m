function []=writeText(outfname,textToWrite);

disp(['writing text to ' outfname]);
fid=fopen(outfname,'w');
for I=1:length(textToWrite)
    fprintf(fid,'%s\n',textToWrite{I});
end
fclose(fid);
