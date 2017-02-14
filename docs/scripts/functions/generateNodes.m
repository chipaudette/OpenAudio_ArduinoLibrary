function [nodes] = generateNodes(origNode_fname,newNode_pname,outfname)

if nargin < 3
    %outfname = 'NewOutputs\new_nodes.txt';
    outfname = [];
    if nargin < 2
        %source location for header files for all of the new nodes
        newNode_pname = 'C:\Users\wea\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary\';
        if nargin < 2
            %location of original node text from original index.html
            origNode_fname = 'ParsedInputs\nodes.txt';
        end
    end
end


addpath('functions\');

%% read existing node file and get the nodes
orig_nodes = parseNodeFile(origNode_fname);

% keep just a subset of the nodes
% nodes_keep = {'AudioInputI2S','AudioInputUSB',...
%     'AudioOutputI2S','AudioOutputUSB',...
%     'AudioPlaySdWav',...
%     'AudioPlayQueue','AudioRecordQueue',...
%     'AudioSynthWaveformSine','AudioSynthWaveform','AudioSynthToneSweep',...
%     'AudioSynthNoiseWhite','AudioSynthNoisePink',...
%     'AudioAnalyzePeak','AudioAnalyzeRMS',...
%     'AudioControlSGTL5000'};
%
% nodes_keep = {'AudioInputUSB',...
%     'AudioOutputUSB',...
%     'AudioPlaySdWav',...
%     'AudioPlayQueue','AudioRecordQueue',...
%     'AudioAnalyzePeak','AudioAnalyzeRMS'};

nodes_keep = {
    'AudioControlSGTL5000',...
    'AudioInputUSB',...
    'AudioOutputUSB',...
    };

%adjust node shortnames
for Inode=1:length(orig_nodes)
    node = orig_nodes(Inode);
    if strcmpi(node.type,'AudioInputUSB');
        node.shortName = 'usbAudioIn';
    elseif strcmpi(node.type,'AudioOutputUSB');
        node.shortName = 'usbAudioOut';
    end
    orig_nodes(Inode)=node;
end

%adjust node icons
for Inode=1:length(orig_nodes)
    node = orig_nodes(Inode);
    if strcmpi(node.type,'AudioControlSGTL5000')
        node.icon = 'debug.png';
    end
    orig_nodes(Inode)=node;
end

%keep just these
nodes=[];
for Ikeep=1:length(nodes_keep)
    for Iorig=1:length(orig_nodes)
            node = orig_nodes(Iorig);
        if strcmpi(node.type,nodes_keep{Ikeep})
            if isempty(nodes)
                nodes = node;
            else
                nodes(end+1) = node;
            end
        end
    end
end

%% add in new nodes

%To build text for the new nodes, use buildNewNodes.m.
%Then paste into XLSX to edit as desired.
%Then load the XLSX via the command below
if (0)
    [num,txt,raw]=xlsread('myNodes.xlsx');
    headings = raw(1,:);
    new_node_data = raw(2:end,:);
else
    %get info directly from underlying classes
    %source_pname = 'C:\Users\wea\Documents\Arduino\libraries\OpenAudio_ArduinoLibrary\';
    [headings, new_node_data]=buildNewNodes(newNode_pname);
end

%build new node data elements
template = nodes(1);
new_nodes=[];
for Inode = 1:size(new_node_data,1)
    node = template;
    for Iheading = 1:length(headings)
        node.(headings{Iheading}) = new_node_data{Inode,Iheading};
    end
    
    if isempty(nodes)
        nodes = node;
    else
        nodes(end+1) = node;
    end
end
clear new_node_data

%remove some undesired nodes
remove_names = {'AudioControlSGTL5000_Extended'};
Ikeep = ones(size(nodes));
for Irem=1:length(remove_names)
    for Inode = 1:length(Ikeep)
        if strcmpi(nodes(Inode).type,remove_names{Irem})
            Ikeep(Inode)=0;
        end
    end
end
Ikeep = find(Ikeep);
nodes = nodes(Ikeep);

%% put some of the nodes into a particular desired order
first_second = {};
first_second(end+1,:) ={'tlv320aic3206' 'sgtl5000'};
first_second(end+1,:) ={'inputI2S' 'usbAudioIn'};
first_second(end+1,:) ={'outputI2S' 'usbAudioOut'};
first_second(end+1,:) ={'i2sAudioIn' 'usbAudioIn'};
first_second(end+1,:) ={'i2sAudioOut' 'usbAudioOut'};

for Iswap = 1:length(first_second);
    all_names = {nodes(:).shortName};
    I = find(strcmpi(all_names,first_second{Iswap,1}));
    J = find(strcmpi(all_names,first_second{Iswap,2}));
    if ~isempty(I) & ~isempty(J)
        first_node = nodes(I); second_node = nodes(J);
        nodes(min([I(1) J(1)])) = first_node;  %this comes first
        nodes(max([I(1) J(1)])) = second_node;  %this comes second
    end
end

%% write new nodes
if ~isempty(outfname)
    writeNodeText(nodes,outfname)
end



