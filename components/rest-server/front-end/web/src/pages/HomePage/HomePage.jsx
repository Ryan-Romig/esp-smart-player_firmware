import { useEffect, useState } from "react"

function HomePage() {
    const [data, setData] = useState("none")
    const handleFormSubmit = async(e) => {
        e.preventDefault()
        const formEntries = new FormData(e.target)
        const json = JSON.stringify(Object.fromEntries(formEntries))
        fetch('api/post', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: json, // replace with your actual data
        })
        .then(() => fetchData())
        .catch((error) => {
            console.error('Error:', error);
        });

    }
    const fetchData = () => {
        fetch('api/get')
        .then(response => response.json())
        .then(data => setData(data.time))
        .catch((error) => {
            console.error('Error:', error);
        });
    }
    useEffect(() => {
       fetchData()
    }, [])
    return (
        <div>
{data}
            <form onSubmit={handleFormSubmit}>
                <input name="wifi_ssid"></input>
                <input name="wifi_password"></input>
                <button type="submit">Submit</button>
            </form>
        </div>
    );
}

export default HomePage;
