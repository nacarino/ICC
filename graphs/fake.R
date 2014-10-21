
suppressPackageStartupMessages(library (ggplot2))
suppressPackageStartupMessages(library (scales))
suppressPackageStartupMessages(library (optparse))
suppressPackageStartupMessages(library (doBy))

# set some reasonable defaults for the options that are needed
option_list <- list (
# make_option(c("-f", "--file"), type="character", default="results/fake/5/rate-trace",
#           help="File which holds the raw rate data.\n\t\t[Default \"%default\"]"),)
#make_option(c("-o", "--output"), type="character", default=".",
#            help="Output directory for graphs.\n\t\t[Default \"%default\"]"),
#make_option(c("-t", "--title"), type="character", default="Scenario",
#           help="Title for the graph")


#opt = parse_args(OptionParser(option_list=option_list, description="Creates graphs from ndnSIM L3 Data rate Tracer data"))

speed = c(0, 5, 10)
mode = c("proactive", "normal")
distance = c(50, 100, 200)
output <- matrix(1:24, nrow = 18, ncol = 4)
count <- 1

    # Load the parser 
for(d in distance){
  for(s in speed){
    for(m in mode){
      
    fileName = paste("../results", m, s, d,"rate-trace", sep = "/")
    data = read.table(fileName, header = T)

    # Filter for a mobile node 
    data <- data[data$Node == 0,]
    data <- data[data$FaceId == 0,]
    data <- data[data$Type == "InData",]
    
    data$Kilobits <- data$Kilobytes * 8
    endTime <- max(data[,"Time"])
    averageDataRate <- sum(data[,"Kilobits"])/endTime
    output[count,] <- c(d,s,m,averageDataRate)
    count <- count + 1
    }
  }
}

colnames(output) <- c("Distance","Speed","Mode", "DataRate") 


    # Draw the graphs 
for(d in distance){

  intname = paste("MN Data rate distance=", d)
  plotData2 = as.data.frame(output)
  plotData = subset (plotData2, Distance == d)
  plotData$Speed = as.numeric(as.character(plotData$Speed))
  plotData$DataRate = as.numeric(as.character(plotData$DataRate))
  plotData$Mode = factor(plotData$Mode)

  g.int <- ggplot (plotData, aes(color=Mode, group=Mode) ) + 
    geom_line(aes(x=Speed, y=DataRate, linetype=Mode), size=1.5, colour="black" lineType="dashed") +
    geom_point(aes(x=Speed, y=DataRate, shape=Mode), size=5, colour="#CC0000") +

    # Font settings
    theme(title = element_text(size=35, colour="black"), 
          axis.title.x = element_text(size=30, colour="black"), axis.title.y = element_text(size=30, colour="black"),
          axis.text.x = element_text(size=24, colour="black"),axis.text.y = element_text(size=24, colour="black"),
          legend.title = element_text(size=26, colour="black"), legend.text = element_text(size=26, colour="black"))+
    ggtitle (intname) +
    ylab ("Rate [Kbits/s]") +
    xlab ("Mobile Node's speed [Km/h]") 

  # make png file
outInpng = sprintf("./%s.png", intname)
png (outInpng, width=1024, height=768)
print (g.int)
x = dev.off ()
}

