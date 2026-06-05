package main

import "fmt"

type Dog struct {
	Name string
}

func (d Dog) Bark() string {
	return "Woof! I am " + d.Name
}

func Add(a, b int) int {
	return a + b
}

func main() {
	d := Dog{Name: "Rex"}
	fmt.Println(d.Bark())
}
