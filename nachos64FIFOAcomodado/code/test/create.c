int main(){
	int fd;
	char  buf[6];
	Create("archivo.nuevo");
	fd = Open("archivo.nuevo");
	Write("prueba", 6, fd);
	Close(fd);
	Exec("../test/brillo");	
	Exec("brillo1");

	fd = Open("archivo.nuevo");
	Read(buf, 6, fd);
	Write(buf, 6, 1);

	Exit(0);
	fd = 1;
	fd++;
	return 0;
}
